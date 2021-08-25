
#include "ParseConfig.h"
#include "string_iteration.h"
#include "Key.h"
#include <cassert>
#include <cctype>
#include <istream>
#include <algorithm>
#include <iterator>
#include <sstream>

namespace {

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

  bool contains(const KeySequence& sequence, KeyCode key) {
    return std::find_if(cbegin(sequence), cend(sequence),
      [&](const KeyEvent& event) {
        return event.key == key;
      }) != cend(sequence);
  }

  void replace_key(KeySequence& sequence, KeyCode both, KeyCode key) {
    std::for_each(begin(sequence), end(sequence),
      [&](KeyEvent& event) {
        if (event.key == both)
          event.key = key;
      });
  }

  void replace_modifier(Command& command, KeyCode both, KeyCode key) {
    replace_key(command.input, both, key);
    replace_key(command.default_mapping, both, key);
    for (auto& mapping : command.context_mappings)
      replace_key(mapping.output, both, key);
  }

  void replace_not_key(KeySequence& sequence, KeyCode both, KeyCode left,
                       KeyCode right) {
    for (auto it = begin(sequence); it != end(sequence); ++it)
      if (it->state == KeyState::Not && it->key == both) {
        it->key = right;
        it = sequence.insert(it, { left, KeyState::Not });
        ++it;
      }
  }

  void replace_not_modifier(Command& command, KeyCode both, KeyCode left,
                            KeyCode right) {
    replace_not_key(command.input, both, left, right);
    replace_not_key(command.default_mapping, both, left, right);
    for (auto& mapping : command.context_mappings)
      replace_not_key(mapping.output, both, left, right);
  }

  void remove_mappings_to_context(Command& command, int context_index, bool apply_default) {
    for (auto it = begin(command.context_mappings); it != end(command.context_mappings); ) {
      auto& mapping = *it;
      if (mapping.context_index == context_index) {
        if (apply_default)
          command.default_mapping = mapping.output;
        it = command.context_mappings.erase(it);
      }
      else {
        // reduce indices pointing to following contexts
        if (mapping.context_index > context_index)
          --mapping.context_index;
        ++it;
      }
    }
  }

  std::string generate_unique_name_from_sequence(const KeySequence& sequence) {
    auto ss = std::stringstream();
    ss << '#';
    for (const auto& event : sequence)
      ss << event.key << '/' << static_cast<int>(event.state) << ',';
    return ss.str();
  }
} // namespace

Config ParseConfig::operator()(std::istream& is, bool add_default_mappings) {
  m_line_no = 0;
  m_config.commands.clear();
  m_config.contexts.clear();
  m_commands_mapped.clear();
  m_macros.clear();

  if (add_default_mappings) {
    // add mappings for immediately passing on common modifiers
    for (auto key : { Key::Shift, Key::Control, Key::AltLeft, Key::AltRight })
      add_mapping( { { *key, KeyState::Down } }, { { *key, KeyState::Down } });
  }

  auto line = std::string();
  while (is.good()) {
    std::getline(is, line);
    ++m_line_no;
    parse_line(cbegin(line), cend(line));
  }

  // check if there is a mapping for each command (to reduce typing errors)
  for (const auto& kv : m_commands_mapped)
    if (!kv.second)
      throw ParseError("Command '" + kv.first + "' was not mapped");

  // remove contexts of other systems
  // and apply contexts without class and title filter immediately
  auto context_index = 0;
  for (auto it = begin(m_config.contexts); it != end(m_config.contexts); ) {
    auto& context = *it;
    if (!context.system_filter_matched ||
        (!context.window_class_filter &&
         !context.window_title_filter)) {
      const auto apply_default = context.system_filter_matched;
      for (auto& command : m_config.commands)
        remove_mappings_to_context(command, context_index, apply_default);
      it = m_config.contexts.erase(it);
    }
    else {
      ++it;
      ++context_index;
    }
  }

  replace_logical_modifiers(*Key::Shift, *Key::ShiftLeft, *Key::ShiftRight);
  replace_logical_modifiers(*Key::Control, *Key::ControlLeft, *Key::ControlRight);
  replace_logical_modifiers(*Key::Meta, *Key::MetaLeft, *Key::MetaRight);

  return std::move(m_config);
}

void ParseConfig::error(std::string message) {
  throw ParseError(std::move(message) +
    " in line " + std::to_string(m_line_no));
}

void ParseConfig::parse_line(It it, const It end) {
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
      parse_macro(std::move(first_ident), it, end);
    }
    else if (skip(&it, end, ">>")) {
      if (has_command(first_ident))
        parse_mapping(std::move(first_ident), it, end);
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

Filter ParseConfig::read_filter(It* it, const It end) {
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
    auto type = std::regex::ECMAScript;
    const auto expr = std::string(begin, *it);
    if (skip(it, end, "i"))
      type |= std::regex::icase;
    return Filter{ expr, std::regex(expr.substr(1, expr.size() - 2), type) };
  }
  else {
    // a string
    if (skip(it, end, "'") || skip(it, end, "\"")) {
      const char mark[2] = { *(*it - 1), '\0' };
      if (!skip_until(it, end, mark))
        error("Unterminated string");
      return Filter{ std::string(begin + 1, *it - 1), { } };
    }
    skip_value(it, end);
    return Filter{ std::string(begin, *it), { } };
  }
}

void ParseConfig::parse_context(It* it, const It end) {
  skip_space(it, end);

  // TODO: for backward compatibility, remove
  skip(it, end, "window");
  skip(it, end, "Window");
  skip_space(it, end);

  auto system_filter_matched = true;
  auto class_filter = Filter();
  auto title_filter = Filter();
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
    else if (attrib == "system") {
      system_filter_matched =
        (to_lower(read_value(it, end)) == current_system);
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

  m_config.contexts.push_back({
    system_filter_matched,
    std::move(class_filter),
    std::move(title_filter)
  });
}

void ParseConfig::parse_mapping(std::string name, It begin, It end) {
  add_mapping(std::move(name), parse_output(begin, end));
}

bool is_ident(const std::string& string) {
  auto it = string.begin();
  const auto end = string.end();
  skip_ident(&it, end);
  return (it == end);
}

std::string ParseConfig::parse_command_name(It it, It end) const {
  skip_space(&it, end);
  auto ident = preprocess_ident(read_ident(&it, end));
  skip_space(&it, end);
  if (it != end ||
      !is_ident(ident) ||
      get_key_by_name(ident) != Key::None)
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
  try {
    return m_parse_sequence(preprocess(it, end), true);
  }
  catch (const std::exception& ex) {
    error(ex.what());
  }
}

KeyCode ParseConfig::add_terminal_command_action(std::string_view command) {
  const auto action_key_code =
    static_cast<KeyCode>(first_action_key + m_config.actions.size());
  m_config.actions.push_back({ std::string(command) });
  return action_key_code;
}

KeySequence ParseConfig::parse_output(It it, It end) {
  skip_space(&it, end);
  try {
    return m_parse_sequence(preprocess(it, end), false,
      std::bind(&ParseConfig::add_terminal_command_action,
                this, std::placeholders::_1));
  }
  catch (const std::exception& ex) {
    error(ex.what());
  }
}

void ParseConfig::parse_macro(std::string name, It it, const It end) {
  if (get_key_by_name(name) != Key::None)
    error("Invalid macro name '" + name + "'");
  skip_space(&it, end);

  // we can safely trim here, because macro cannot have a terminal command
  It trimmed = end;
  trim_comment(it, &trimmed);
  m_macros[std::move(name)] = preprocess(it, trimmed);
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

bool ParseConfig::has_command(const std::string& name) const {
  const auto& commands = m_config.commands;
  return (std::find_if(cbegin(commands), cend(commands),
    [&](const Command& c) { return c.name == name; }) != cend(commands));
}

void ParseConfig::add_command(KeySequence input, std::string name) {
  assert(!name.empty());
  if (!m_config.contexts.empty())
    error("Cannot add command in context");
  if (has_command(name))
    error("Duplicate command '" + name + "'");

  m_config.commands.push_back({ name, std::move(input), {}, {} });
  m_commands_mapped[std::move(name)] = false;
}

void ParseConfig::add_mapping(KeySequence input, KeySequence output) {
  assert(!input.empty());
  // assign a unique name per input sequence
  auto name = generate_unique_name_from_sequence(input);
  if (m_config.contexts.empty()) {
    // creating mapping in default context, set default output expression
    m_config.commands.push_back({ std::move(name), std::move(input), std::move(output), {} });
  }
  else if (m_config.contexts.back().system_filter_matched) {
    // mapping sequence in context, try to override existing command
    if (!std::count_if(m_config.commands.begin(), m_config.commands.end(),
          [&](const Command& command) { return command.name == name; })) {
      // create command with forwarding default mapping
      const auto default_mapping = KeySequence{ { any_key, KeyState::Down } };
      m_config.commands.push_back({ name, std::move(input), std::move(default_mapping), {} });
    }
    add_mapping(std::move(name), std::move(output));
  }
}

void ParseConfig::add_mapping(std::string name, KeySequence output) {
  assert(!name.empty());
  const auto it = std::find_if(
    begin(m_config.commands), end(m_config.commands),
    [&](const Command& command) { return command.name == name; });
  if (it == cend(m_config.commands))
    error("Unknown command '" + name + "'");

  if (!m_config.contexts.empty()) {
    // set mapping override
    const auto context_index = static_cast<int>(m_config.contexts.size()) - 1;
    if (!it->context_mappings.empty() &&
        it->context_mappings.back().context_index == context_index)
      error("Duplicate mapping override for '" + name + "'");
    it->context_mappings.push_back({ context_index, std::move(output) });
  }
  else {
    // set context default mapping
    if (!it->default_mapping.empty())
      error("Duplicate mapping of '" + name + "'");
    it->default_mapping = std::move(output);
  }
  m_commands_mapped[name] = true;
}

void ParseConfig::replace_logical_modifiers(KeyCode both, KeyCode left,
    KeyCode right) {
  auto& commands = m_config.commands;
  for (auto it = begin(commands); it != end(commands); ) {
    // replace !Shift with !LeftShift !RightShift
    replace_not_modifier(*it, both, left, right);

    if (contains(it->input, both)) {
      // duplicate command and replace the logical with a physical key
      it = commands.insert(it, *it);
      replace_modifier(*it++, both, left);
      replace_modifier(*it++, both, right);
    }
    else {
      // still convert all logical to physical keys in output
      replace_modifier(*it++, both, left);
    }
  }
}
