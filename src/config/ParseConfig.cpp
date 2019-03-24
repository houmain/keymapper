
#include "ParseConfig.h"
#include "string_iteration.h"
#include "key_names.h"
#include <cassert>
#include <cctype>
#include <istream>
#include <algorithm>
#include <iterator>

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

  void replace_modifier(Command& command, Key both, Key key) {
    replace_key(command.input, both, key);
    replace_key(command.default_mapping, both, key);
    for (auto& mapping : command.context_mappings)
      replace_key(mapping.output, both, key);
  }

  void replace_not_key(KeySequence& sequence, Key both, Key left, Key right) {
    for (auto it = begin(sequence); it != end(sequence); ++it)
      if (it->state == KeyState::Not && it->key == both) {
        it->key = right;
        it = sequence.insert(it, { left, KeyState::Not });
        ++it;
      }
  }

  void replace_not_modifier(Command& command, Key both, Key left, Key right) {
    replace_not_key(command.input, both, left, right);
    replace_not_key(command.default_mapping, both, left, right);
    for (auto& mapping : command.context_mappings)
      replace_not_key(mapping.output, both, left, right);
  }
} // namespace

Config ParseConfig::operator()(std::istream& is) {
  m_line_no = 0;
  m_config.commands.clear();
  m_config.contexts.clear();
  m_commands_mapped.clear();
  m_macros.clear();

  // automatically add mappings for common modifiers
  for (auto key : { Key::SHIFT, Key::CTRL, Key::LEFTALT, Key::RIGHTALT })
    add_mapping({ { key, KeyState::Down } }, { { key, KeyState::Down } });

  auto line = std::string();
  while (is.good()) {
    std::getline(is, line);
    ++m_line_no;
    parse_line(cbegin(line), cend(line));
  }

  // check if there is a mapping for each command (to reduce typing errors)
  for (const auto& kv : m_commands_mapped)
    if (!kv.second)
      throw ParseError("command '" + kv.first + "' was not mapped");

  replace_logical_modifiers(Key::SHIFT, Key::LEFTSHIFT, Key::RIGHTSHIFT);
  replace_logical_modifiers(Key::CTRL, Key::LEFTCTRL, Key::RIGHTCTRL);
  replace_logical_modifiers(Key::META, Key::LEFTMETA, Key::RIGHTMETA);
  replace_any_key_in_output();

  return std::move(m_config);
}

void ParseConfig::error(std::string message) {
  throw ParseError(std::move(message) +
    " (in line " + std::to_string(m_line_no) + ")");
}

void ParseConfig::parse_line(It it, const It end) {
  skip_space_and_comments(&it, end);
  if (it == end)
    return;

  if (skip(&it, end, "[")) {
    const auto begin = it;
    if (!skip_until(&it, end, "]"))
      error("missing ']'");

    parse_directive(begin, it-1);
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
        error("missing '>>'");

      parse_command_and_mapping(begin, it - 2, it, end);
    }
    it = end;
  }

  skip_space_and_comments(&it, end);
  if (it != end)
    error("unexpected '" + std::string(it, end) + "'");
}

void ParseConfig::parse_directive(It it, const It end) {
  skip_space(&it, end);
  if (skip(&it, end, "window")) {
    skip_space(&it, end);

    auto class_filter = std::string();
    auto title_filter = std::string();
    auto system_filter_matched = true;

    while (it != end) {
      const auto attrib = read_ident(&it, end);
      skip_space(&it, end);
      if (!skip(&it, end, "="))
        error("missing '='");

      auto value = read_value(&it, end);
      if (attrib == "class") {
        class_filter = std::move(value);
      }
      else if (attrib == "title") {
        title_filter = std::move(value);
      }
      else if (attrib == "system") {
        system_filter_matched = (to_lower(std::move(value)) == current_system);
      }
      else {
        error("unexpected '" + attrib + "'");
      }
      skip_space(&it, end);
    }
    begin_window(class_filter, title_filter, system_filter_matched);
  }
  else {
    error("unexpected '" + std::string(it, end) + "'");
  }
}

void ParseConfig::parse_mapping(std::string name, It begin, It end) {
  add_mapping(std::move(name), parse_output(begin, end));
}

std::string ParseConfig::parse_command_name(It it, It end) const {
  skip_space(&it, end);
  auto ident = preprocess_ident(read_ident(&it, end));
  if (ident.find(' ') != std::string::npos ||
      get_key_by_name(ident) != Key::NONE)
    return { };
  return ident;
}

void ParseConfig::parse_command_and_mapping(const It in_begin, const It in_end,
    const It out_begin, const It out_end) {
  // if output consists of a single identifier, it is a command name
  auto ident = parse_command_name(out_begin, out_end);
  if (!ident.empty())
    add_command(std::move(ident), parse_input(in_begin, in_end));
  else
    add_mapping(parse_input(in_begin, in_end),
      parse_output(out_begin, out_end));
}

KeySequence ParseConfig::parse_input(It it, It end) {
  skip_space(&it, end);
  trim_comment(it, &end);
  try {
    return m_parse_sequence(preprocess(it, end), true);
  }
  catch (const std::exception& ex) {
    error(ex.what());
  }
}

KeySequence ParseConfig::parse_output(It it, It end) {
  skip_space(&it, end);
  trim_comment(it, &end);
  try {
    return m_parse_sequence(preprocess(it, end), false);
  }
  catch (const std::exception& ex) {
    error(ex.what());
  }
}

void ParseConfig::parse_macro(std::string name, It it, const It end) {
  if (get_key_by_name(name) != Key::NONE)
    error("invalid macro name '" + name + "'");
  skip_space(&it, end);
  m_macros[std::move(name)] = preprocess(it, end);
}

std::string ParseConfig::preprocess_ident(std::string ident) const {
  const auto macro = m_macros.find(ident);
  if (macro != cend(m_macros))
    return macro->second;
  return ident;
}

std::string ParseConfig::preprocess(It it, const It end) const {
  auto result = std::string();
  for (;;) {
    // remove comments
    skip_comments(&it, end);
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

void ParseConfig::add_command(std::string name, KeySequence input) {
  assert(!name.empty());
  if (!m_config.contexts.empty())
    error("cannot add command in context");
  if (has_command(name))
    error("duplicate command '" + name + "'");

  m_config.commands.push_back({ name, std::move(input), {}, {} });
  m_commands_mapped[std::move(name)] = false;
}

void ParseConfig::add_mapping(KeySequence input, KeySequence output) {
  assert(!input.empty());
  if (!m_config.contexts.empty())
    error("cannot map sequence in context");
  m_config.commands.push_back({ "", std::move(input), std::move(output), {} });
}

void ParseConfig::begin_window(std::string class_filter,
    std::string title_filter, bool system_filter_matched) {
  // simply set invalid class when system filter did not match
  if (!system_filter_matched)
    class_filter = "$";

  m_config.contexts.push_back(
    { std::move(class_filter), std::move(title_filter) });
}

void ParseConfig::add_mapping(std::string name, KeySequence output) {
  assert(!name.empty());
  const auto it = std::find_if(
    begin(m_config.commands), end(m_config.commands),
    [&](const Command& command) { return command.name == name; });
  if (it == cend(m_config.commands))
    error("unknown command '" + name + "'");

  if (!m_config.contexts.empty()) {
    // set mapping override
    const auto context_index = static_cast<int>(m_config.contexts.size()) - 1;
    if (!it->context_mappings.empty() &&
        it->context_mappings.back().context_index == context_index)
      error("duplicate mapping override for '" + name + "'");
    it->context_mappings.push_back({ context_index, std::move(output) });
  }
  else {
    // set context default mapping
    if (!it->default_mapping.empty())
      error("duplicate mapping of '" + name + "'");
    it->default_mapping = std::move(output);
  }
  m_commands_mapped[name] = true;
}

void ParseConfig::replace_logical_modifiers(Key both, Key left, Key right) {
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

void ParseConfig::replace_any_key_in_output() {
  for (auto& command : m_config.commands) {
    replace_key(command.default_mapping, Key::ANY, Key::A);
    for (auto& mapping : command.context_mappings)
      replace_key(mapping.output, Key::ANY, Key::A);
  }
}
