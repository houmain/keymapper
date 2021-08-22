#pragma once

#include "runtime/KeyEvent.h"
#include "Config.h"
#include "ParseKeySequence.h"
#include <iosfwd>
#include <map>

class ParseConfig {
public:
  Config operator()(std::istream& is, bool add_default_mappings = true);

private:
  using It = std::string::const_iterator;

  [[noreturn]] void error(std::string message);
  void parse_line(It begin, It end);
  void parse_context(It* begin, It end);
  void parse_macro(std::string name, It begin, It end);
  void parse_mapping(std::string name, It begin, It end);
  std::string parse_command_name(It begin, It end) const;
  void parse_command_and_mapping(It in_begin, It in_end,
                                 It out_begin, It out_end);
  KeySequence parse_input(It begin, It end);
  KeySequence parse_output(It begin, It end);
  std::string preprocess_ident(std::string ident) const;
  std::string preprocess(It begin, It end) const;
  void replace_logical_modifiers(KeyCode both, KeyCode left, KeyCode right);
  Filter read_filter(It* it, It end);
  KeyEvent generate_terminal_command_action(It it, It end);

  bool has_command(const std::string& name) const;
  void add_command(KeySequence input, std::string name);
  void add_mapping(KeySequence input, KeySequence output);
  void add_mapping(std::string name, KeySequence output);

  int m_line_no{ };
  Config m_config;
  std::map<std::string, std::string> m_macros;
  ParseKeySequence m_parse_sequence;
  std::map<std::string, bool> m_commands_mapped;
};
