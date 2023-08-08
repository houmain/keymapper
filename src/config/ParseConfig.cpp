
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
#else
#  error unknown system
#endif

  std::string to_lower(std::string str) {
    for (auto& c : str)
      c = static_cast<char>(std::tolower(c));
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
} // namespace

Config ParseConfig::operator()(std::istream& is) {
  m_line_no = 0;
  m_config = { };
  m_commands.clear();
  m_macros.clear();
  m_logical_keys.clear();
  m_context_modifier.clear();

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
      throw ParseError("Command '" + command.name + "' was not mapped");

  // remove contexts of other systems or which are empty
  m_config.contexts.erase(
    std::remove_if(begin(m_config.contexts), end(m_config.contexts),
      [](const Config::Context& context) {
        return !context.system_filter_matched || 
          (context.inputs.empty() && context.command_outputs.empty());
      }),
    m_config.contexts.end());

  // replace logical keys (in reverse order of registration)
  for (auto it = m_logical_keys.rbegin(); it != m_logical_keys.rend(); ++it) {
    const auto& [name, both, left, right] = *it;
    replace_logical_key(both, left, right);
  }
  return std::move(m_config);
}

void ParseConfig::error(std::string message) {
  throw ParseError(std::move(message) +
    " in line " + std::to_string(m_line_no));
}

void ParseConfig::parse_line(It it, It end) {
  skip_space_and_comments(&it, end);
  if (it == end)
    return;

  if (skip(&it, end, "[")) {
    parse_context(&it, end);
  }
  else {
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
    const char mark[2] = { *(*it - 1), '\0' };
    if (!skip_until(it, end, mark))
      error("Unterminated string");
    return std::string(begin + 1, *it - 1);
  }
  else {
    skip_value(it, end);
    return std::string(begin, *it);
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

void ParseConfig::parse_context(It* it, const It end) {
  skip_space(it, end);

  auto system_filter_matched = true;
  auto class_filter = Config::Filter();
  auto title_filter = Config::Filter();
  auto path_filter = Config::Filter();
  auto device_filter = std::string();
  m_context_modifier = { };

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
      if (!skip(it, end, "="))
        error("Missing '='");

      skip_space(it, end);
      if (attrib == "class") {
        class_filter = read_filter(it, end);
      }
      else if (attrib == "title") {
        title_filter = read_filter(it, end);
      }
      else if (attrib == "path") {
        path_filter = read_filter(it, end);
      }
      else if (attrib == "system") {
        system_filter_matched =
          (to_lower(read_value(it, end)) == current_system);
      }
      else if (attrib == "device") {
        device_filter = read_filter(it, end).string;
        if (device_filter.empty())
          error("String expected");
      }
      else if (attrib == "modifier") {
        auto modifier = read_value(it, end);
        m_context_modifier = m_parse_sequence(
          preprocess(modifier.begin(), modifier.end()), true,
          std::bind(&ParseConfig::get_key_by_name, this, _1));
        m_context_modifier.erase(
          std::remove_if(m_context_modifier.begin(), m_context_modifier.end(),
            [](const KeyEvent& event) { return (event.state == KeyState::UpAsync); }),
          m_context_modifier.end());
      }
      else {
        error("Unexpected '" + attrib + "'");
      }

      skip_space(it, end);
      if (skip(it, end, "]"))
        break;

      skip_space(it, end);
      if (*it == end)
        error("Missing ']'");
    }
  }

  m_config.contexts.push_back({
    system_filter_matched,
    std::move(class_filter),
    std::move(title_filter),
    std::move(path_filter),
    std::move(device_filter)
  });
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

KeySequence ParseConfig::parse_input(It it, It end) try {
  skip_space(&it, end);
  auto sequence = m_parse_sequence(preprocess(it, end), true,
    std::bind(&ParseConfig::get_key_by_name, this, _1));
  sequence.insert(sequence.begin(),
    m_context_modifier.begin(), m_context_modifier.end());
  return sequence;
}
catch (const std::exception& ex) {
  error(ex.what());
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

KeySequence ParseConfig::parse_output(It it, It end) try {
  skip_space(&it, end);
  return m_parse_sequence(preprocess(it, end), false,
    std::bind(&ParseConfig::get_key_by_name, this, _1),
    std::bind(&ParseConfig::add_terminal_command_action, this, _1));
}
catch (const std::exception& ex) {
  error(ex.what());
}

void ParseConfig::parse_macro(std::string name, It it, const It end) {
  if (*get_key_by_name(name))
    error("Invalid macro name '" + name + "'");
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
    skip_ident(&it, end);
    if (begin != it) {
      // match read ident
      result.append(preprocess_ident(std::string(begin, it)));
    }
    else {
      // output single character
      result.append(begin, ++it);
    }
  }
  return result;
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
  for (auto& context : m_config.contexts) {
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
  }
}
