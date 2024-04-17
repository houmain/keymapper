
#include "ParseConfig.h"
#include "string_iteration.h"
#include "runtime/Key.h"
#include "common/parse_regex.h"
#include <cassert>
#include <cctype>
#include <istream>
#include <algorithm>
#include <iterator>

namespace {
  using namespace std::placeholders;

#if defined(__linux)
  const auto current_system = "linux";
#elif defined(_WIN32)
  const auto current_system = "windows";
#elif defined(__APPLE__)
  const auto current_system = "macos";
#else
#  error unknown system
#endif

  std::string to_lower(std::string str) {
    for (auto& c : str)
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return str;
  }

  bool contains(const KeySequence& sequence, Key key) {
    return std::find_if(cbegin(sequence), cend(sequence),
      [&](const KeyEvent& event) {
        return event.key == key;
      }) != cend(sequence);
  }

  void replace_key(KeySequence& sequence, Key both, Key key) {
    std::for_each(begin(sequence), end(sequence),
      [&](KeyEvent& event) {
        if (event.key == both)
          event.key = key;
      });
  }

  void replace_not_key(KeySequence& sequence, Key both, Key left,
                       Key right) {
    for (auto it = begin(sequence); it != end(sequence); ++it)
      if (it->state == KeyState::Not && it->key == both) {
        it->key = right;
        it = sequence.insert(it, { left, KeyState::Not });
        ++it;
      }
  }

  std::string_view trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
      s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
      s.remove_suffix(1);
    return s;
  }

  std::string_view unquote(std::string_view s) {
    if (s.size() >= 2 &&
        s.front() == s.back() &&
        (s.front() == '\'' || s.front() == '"'))
      s = s.substr(1, s.size() - 2);
    return s;
  }

  std::vector<std::string> get_argument_list(std::string_view list) {
    auto it = list.begin();
    const auto end = list.end();
    auto begin = it;
    auto arguments = std::vector<std::string>();
    while (begin != end) {
      skip_until_not_in_string(&it, end, ",");
      if (it > begin) {
        auto argument = std::string_view(
          &*begin, std::distance(begin, it) - 1);
        arguments.emplace_back(trim(argument));
      }
      begin = it;
    }
    return arguments;
  }

  std::string substitute_arguments(const std::string& text,
      const std::vector<std::string>& arguments) {
    auto it = text.begin();
    const auto end = text.end();
    auto result = std::string();
    for (auto begin = it; it != end; begin = it) {
      if (skip_until(&it, end, '$')) {
        // argument or beginning of terminal command
        result.append(begin, it - 1);
        begin = it - 1;

        if (auto number = try_read_number(&it, end)) {
          const auto index = static_cast<size_t>(*number);
          // ignore missing arguments
          if (index < arguments.size())
            result.append(unquote(arguments[index]));
          continue;
        }
      }
      result.append(begin, it);
    }
    return result;
  }
} // namespace

Config ParseConfig::operator()(std::istream& is) try {
  m_line_no = 0;
  m_config = { };
  m_commands.clear();
  m_macros.clear();
  m_logical_keys.clear();
  m_system_filter_matched = true;
  m_after_empty_context_block = false;

  // add default context
  m_config.contexts.push_back({ true, {}, {} });

  // register common logical keys
  add_logical_key("Shift", Key::ShiftLeft, Key::ShiftRight);
  add_logical_key("Control", Key::ControlLeft, Key::ControlRight);
  add_logical_key("Meta", Key::MetaLeft, Key::MetaRight);
  
  auto line = std::string();
  while (is.good()) {
    std::getline(is, line);
    ++m_line_no;
    parse_line(cbegin(line), cend(line));
  }

  // check if there is a mapping for each command (to reduce typing errors)
  for (const auto& command : m_commands)
    if (!command.mapped)
      throw ConfigError("Command '" + command.name + "' was not mapped");

  // remove contexts of other systems or which are empty
  optimize_contexts();

  // replace logical keys (in reverse order of registration)
  for (auto it = m_logical_keys.rbegin(); it != m_logical_keys.rend(); ++it) {
    const auto& [name, both, left, right] = *it;
    replace_logical_key(both, left, right);
  }

  // collect virtual key aliases
  for (const auto& [name, value] : m_macros)
    if (auto key = get_key_by_name(value); is_virtual_key(key))
      m_config.virtual_key_aliases.emplace_back(name, key);

  return std::move(m_config);
}
catch (const ConfigError&) {
  throw;
}
catch (const std::exception& ex) {
  error(ex.what());
}

void ParseConfig::error(std::string message) const {
  throw ConfigError(std::move(message) +
    " in line " + std::to_string(m_line_no));
}

void ParseConfig::parse_line(It it, It end) {
  skip_space_and_comments(&it, end);
  if (skip(&it, end, "[")) {
    if (m_after_empty_context_block)
      m_config.contexts.back().fallthrough = true;

    parse_context(&it, end);
    m_after_empty_context_block = true;
  }
  else {
    m_after_empty_context_block = false;
    if (it == end)
      return;

    const auto begin = it;
    skip_ident(&it, end);
    auto first_ident = std::string(begin, it);

    skip_space(&it, end);
    if (skip(&it, end, "=")) {
      skip_space(&it, end);
      trim_comment(it, &end);
      if (!parse_logical_key_definition(first_ident, it, end))
        parse_macro(std::move(first_ident), it, end);
    }
    else if (skip(&it, end, ">>")) {
      if (find_command(first_ident))
        parse_mapping(first_ident, it, end);
      else
        parse_command_and_mapping(
          cbegin(first_ident), cend(first_ident), it, end);
    }
    else {
      if (!skip_until(&it, end, ">>"))
        error("Missing '>>'");

      parse_command_and_mapping(begin, it - 2, it, end);
    }
    it = end;
  }

  skip_space_and_comments(&it, end);
  if (it != end)
    error("Unexpected '" + std::string(it, end) + "'");
}

std::string ParseConfig::read_filter_string(It* it, const It end) {
  const auto begin = *it;
  if (skip(it, end, "/")) {
    // a regular expression
    for (;;) {
      if (!skip_until(it, end, "/"))
        error("Unterminated regular expression");
      // check for irregular number of preceding backslashes
      auto prev = std::prev(*it, 2);
      while (prev != begin && *prev == '\\')
        prev = std::prev(prev);
      if (std::distance(prev, *it) % 2 == 0)
        break;
    }
    skip(it, end, "i");
    return std::string(begin, *it);
  }
  else if (skip(it, end, "'") || skip(it, end, "\"")) {
    // a string
    if (!skip_until(it, end, *std::prev(*it)))
      error("Unterminated string");
    return std::string(begin + 1, *it - 1);
  }
  else {
    skip_value(it, end);
    auto ident = preprocess_ident(std::string(begin, *it));

    // trim quotes after macro substitution
    if (!ident.empty() && (ident.front() == '\'' || ident.front() == '\"')) {
      if (ident.size() < 2 || ident.back() != ident.front())
        error("Unterminated string");
      ident = ident.substr(1, ident.size() - 2);
    }
    return ident;
  }
}

Config::Filter ParseConfig::read_filter(It* it, const It end) {
  auto string = read_filter_string(it, end);
  if (is_regex(string)) {
    auto regex = parse_regex(string);
    return { std::move(string), std::move(regex) };
  }
  return { std::move(string), { } };
}

KeySequence ParseConfig::parse_modifier_list(std::string_view string) {
  auto it = string.begin();
  const auto end = string.end();
  auto sequence = KeySequence();
  for (;;) {
    skip_space(&it, end);
    if (it == end)
      break;
    const auto is_not = skip(&it, end, "!");
    const auto name = read_ident(&it, end);
    const auto key = get_key_by_name(name);
    if (!*key)
      error("Invalid key name '" + name + "'");
    sequence.emplace_back(key, (is_not ? KeyState::Not : KeyState::Down));
  }
  if (contains(sequence, Key::ContextActive))
    error("Not allowed key ContextActive");
  return sequence;
}

void ParseConfig::parse_context(It* it, const It end) {
  auto& context = m_config.contexts.emplace_back();
  context.system_filter_matched = true;

  skip_space(it, end);
  if (skip(it, end, "default")) {
    skip_space(it, end);
    if (!skip(it, end, "]"))
      error("Missing ']'");
  }
  else {
    for (;;) {
      const auto attrib = read_ident(it, end);
      if (attrib.empty())
        error("Identifier expected");

      skip_space(it, end);
      const auto invert = skip(it, end, "!");
      if (!skip(it, end, "="))
        error("Missing '='");

      skip_space(it, end);
      if (attrib == "class") {
        context.window_class_filter = read_filter(it, end);
        context.window_class_filter.invert = invert;
      }
      else if (attrib == "title") {
        context.window_title_filter = read_filter(it, end);
        context.window_title_filter.invert = invert;
      }
      else if (attrib == "path") {
        context.window_path_filter = read_filter(it, end);
        context.window_path_filter.invert = invert;
      }
      else if (attrib == "system") {
        const auto system = to_lower(read_value(it, end));
        context.system_filter_matched = (system == current_system) ^ invert;
      }
      else if (attrib == "device") {
        context.device_filter = read_filter(it, end).string;
        if (context.device_filter.empty())
          error("String expected");
        context.invert_device_filter = invert;
      }
      else if (attrib == "modifier") {
        const auto modifier = preprocess(read_value(it, end));
        context.modifier_filter = parse_modifier_list(modifier);
        context.invert_modifier_filter = invert;
      }
      else {
        error("Unexpected '" + attrib + "'");
      }

      skip_space(it, end);
      if (skip(it, end, "]"))
        break;

      // allow to separate with commas
      skip(it, end, ',');

      skip_space(it, end);
      if (*it == end)
        error("Missing ']'");
    }
  }
  m_system_filter_matched = context.system_filter_matched;
}

void ParseConfig::parse_mapping(const std::string& name, It begin, It end) {
  add_mapping(name, parse_output(begin, end));
}

bool is_ident(const std::string& string) {
  auto it = string.begin();
  const auto end = string.end();
  skip_ident(&it, end);
  return (it == end);
}

std::string ParseConfig::parse_command_name(It it, It end) const {
  trim_comment(it, &end);
  skip_space(&it, end);
  auto ident = preprocess_ident(read_ident(&it, end));
  skip_space(&it, end);
  if (it != end ||
      !is_ident(ident) ||
      *get_key_by_name(ident))
    return { };
  return ident;
}

void ParseConfig::parse_command_and_mapping(const It in_begin, const It in_end,
    const It out_begin, const It out_end) {
  auto input = parse_input(in_begin, in_end);
  auto command_name = parse_command_name(out_begin, out_end);
  if (!command_name.empty())
    add_command(std::move(input), std::move(command_name));
  else
    add_mapping(std::move(input), parse_output(out_begin, out_end));
}

KeySequence ParseConfig::parse_input(It it, It end) {
  skip_space(&it, end);
  return m_parse_sequence(preprocess(it, end), true,
    std::bind(&ParseConfig::get_key_by_name, this, _1));
}

Key ParseConfig::get_key_by_name(std::string_view name) const {
  if (const auto key = ::get_key_by_name(name); key != Key::none)
    return key;

  const auto it = std::find_if(m_logical_keys.begin(), m_logical_keys.end(),
    [&](const LogicalKey& key) { return key.name == name; });
  if (it != m_logical_keys.end())
    return it->both;

  return { };
}

Key ParseConfig::add_terminal_command_action(std::string_view command) {
  const auto action_key_code =
    static_cast<Key>(*Key::first_action + m_config.actions.size());
  m_config.actions.push_back({ std::string(command) });
  return action_key_code;
}

KeySequence ParseConfig::parse_output(It it, It end) {
  skip_space(&it, end);
  auto sequence = m_parse_sequence(preprocess(it, end), false,
    std::bind(&ParseConfig::get_key_by_name, this, _1),
    std::bind(&ParseConfig::add_terminal_command_action, this, _1));
  if (contains(sequence, Key::ContextActive))
    error("Not allowed key ContextActive");
  return sequence;
}

void ParseConfig::parse_macro(std::string name, It it, const It end) {
  if (*get_key_by_name(name))
    error("Invalid macro name '" + name + "'");
  if (m_system_filter_matched)
    m_macros[std::move(name)] = preprocess(it, end);
}

bool ParseConfig::parse_logical_key_definition(
    const std::string& logical_name, It it, const It end) {
  if (*get_key_by_name(logical_name))
    return false;

  auto left = get_key_by_name(preprocess_ident(read_ident(&it, end)));
  skip_space(&it, end);
  if (!*left || !skip(&it, end, "|"))
    return false;

  for (;;) {
    skip_space(&it, end);
    const auto name = preprocess_ident(read_ident(&it, end));
    const auto right = get_key_by_name(name);
    if (!*right)
      error("Invalid key '" + name + "'");
    skip_space(&it, end);
    if (skip(&it, end, "|")) {
      left = add_logical_key("$", left, right);
      continue;
    }
    add_logical_key(logical_name, left, right);
    if (it != end)
      error("Unexpected '" + std::string(it, end) + "'");
    break;
  }
  return true;
}

std::string ParseConfig::preprocess_ident(std::string ident) const {
  if (auto pos = ident.find('['); pos != std::string::npos) {
    // macro with arguments
    const auto name = ident.substr(0, pos);
    const auto macro = m_macros.find(name);
    if (macro == cend(m_macros))
      error("Unknown macro '" + name + "'");

    // preprocess arguments
    auto arguments = get_argument_list(ident.substr(pos + 1));
    for (auto& argument : arguments)
      argument = preprocess(argument);

    // substitute $0... in macro text with arguments
    return substitute_arguments(macro->second, arguments);
  }

  const auto macro = m_macros.find(ident);
  if (macro != cend(m_macros))
    return macro->second;

  return ident;
}

std::string ParseConfig::preprocess(It it, const It end) const {
  auto result = std::string();
  // remove comments
  skip_space_and_comments(&it, end);

  for (;;) {
    if (it == end)
      break;

    // try to read ident
    auto begin = it;
    if (!skip_ident_with_arglist(&it, end))
      error("Incomplete argument list");

    if (begin != it) {
      // match read ident
      result.append(preprocess_ident(std::string(begin, it)));
    }
    else if (*it == '\'' || *it == '\"') {
      // a string
      if (!skip_until(&it, end, *it++))
        error("Unterminated string");

      result.append(begin, it);
    }
    else {
      // single character
      result.append(begin, ++it);
    }
  }
  return result;
}

std::string ParseConfig::preprocess(const std::string& string) const {
  return preprocess(string.begin(), string.end());
}

Config::Context& ParseConfig::current_context() {
  assert(!m_config.contexts.empty());
  return m_config.contexts.back();
}

auto ParseConfig::find_command(const std::string& name) -> Command* {
  const auto it = std::find_if(
    begin(m_commands), end(m_commands),
    [&](const Command& c) { return c.name == name; });
  return (it != cend(m_commands) ? &*it : nullptr);
}

void ParseConfig::add_command(KeySequence input, std::string name) {
  assert(!name.empty());
  auto& context = current_context();
  auto command = find_command(name);
  if (!command) {
    // command outputs have a negative index
    const auto output_index = -static_cast<int>(m_commands.size() + 1);
    m_commands.push_back({ std::move(name), output_index, false });
    command = &m_commands.back();
  }
  context.inputs.push_back({ std::move(input), command->index });
}

void ParseConfig::add_mapping(KeySequence input, KeySequence output) {
  assert(!input.empty());
  auto& context = current_context();
  context.inputs.push_back({
    std::move(input),
    static_cast<int>(context.outputs.size())
  });
  context.outputs.push_back(std::move(output));
}

void ParseConfig::add_mapping(const std::string& name, KeySequence output) {
  assert(!name.empty());
  auto& context = current_context();
  auto command = find_command(name);
  if (!command)
    error("Unknown command '" + name + "'");

  if (std::count_if(begin(context.command_outputs), end(context.command_outputs),
      [&](const Config::CommandOutput& output) { return output.index == command->index; }))
    error("Duplicate mapping of '" + name + "'");

  context.command_outputs.push_back({
    std::move(output),
    command->index
  });
  command->mapped = true;
}

Key ParseConfig::add_logical_key(std::string name, Key left, Key right) {
  const auto both = static_cast<Key>(*Key::first_logical + m_logical_keys.size());
  m_logical_keys.push_back({ std::move(name), both, left, right });
  return both;
}

void ParseConfig::replace_logical_key(Key both, Key left, Key right) {
  auto& contexts = m_config.contexts;
  for (auto& context : contexts) {
    // replace !<both> with !<left> !<right>
    for (auto& input : context.inputs)
      replace_not_key(input.input, both, left, right);
    for (auto& output : context.outputs)
      replace_not_key(output, both, left, right);
    for (auto& command : context.command_outputs)
      replace_not_key(command.output, both, left, right);

    // duplicate command and replace the logical with a physical key
    for (auto it = begin(context.inputs); it != end(context.inputs); ++it)
      if (contains(it->input, both)) {
        // duplicate and replace with <left> and <right>
        it = context.inputs.insert(it, *it);
        replace_key(it->input, both, left);
        ++it;
        replace_key(it->input, both, right);

        // when directly mapped output also contains logical key,
        // then duplicate output and replace with <right>
        if (it->output_index >= 0) {
          auto& output = context.outputs[it->output_index];
          if (contains(output, both)) {
            it->output_index = static_cast<int>(context.outputs.size());
            context.outputs.push_back(output);
            replace_key(context.outputs.back(), both, right);
          }
        }
      }

    // replace logical key with <left>
    for (auto& output : context.outputs)
      replace_key(output, both, left);
    for (auto& command : context.command_outputs)
      replace_key(command.output, both, left);

    // replace !<both> with !<left> !<right> in context modifier
    replace_not_key(context.modifier_filter, both, left, right);
  }

  // insert fallthrough context and replace the logical with a physical key
  for (auto it = begin(contexts); it != end(contexts); ++it) {
    if (contains(it->modifier_filter, both)) {
      // duplicate and replace with <left> and <right>
      it = contexts.insert(it, *it);
      it->inputs.clear();
      it->outputs.clear();
      it->command_outputs.clear();
      it->fallthrough = true;
      replace_key(it->modifier_filter, both, left);
      ++it;
      replace_key(it->modifier_filter, both, right);
    }
  }
}

void ParseConfig::optimize_contexts() {
  auto& contexts = m_config.contexts;
  auto before_context = false;
  for (auto i = static_cast<int>(contexts.size()) - 1; i >= 0; --i) {
    const auto& context = contexts[i];
    const auto can_not_match = !context.system_filter_matched;
    if (context.fallthrough) {
      // remove fallthrough contexts which are not before a context
      if (can_not_match || !before_context)
        contexts.erase(std::next(contexts.begin(), i));
    }
    else {
      const auto has_no_effect = (context.inputs.empty() && context.command_outputs.empty());
      if (can_not_match || has_no_effect) {
        // convert fallthrough context when removing the non-fallthrough context
        if (i > 0 && contexts[i - 1].fallthrough) {
          auto& before = contexts[i - 1];
          before.inputs = std::move(context.inputs);
          before.outputs = std::move(context.outputs);
          before.command_outputs = std::move(context.command_outputs);
          before.fallthrough = false;
        }
        contexts.erase(std::next(contexts.begin(), i));

        // no longer before a context when removing a non-fallthrough context
        before_context = false;
      }
      else {
        before_context = true;
      }
    }
  }
}
