#pragma once

#include "runtime/KeyEvent.h"
#include "Config.h"
#include "ParseKeySequence.h"
#include <iosfwd>
#include <map>

class ParseConfig {
public:
  struct ConfigError : std::runtime_error {
    using std::runtime_error::runtime_error;
  };

  Config operator()(std::istream& is);

private:
  struct Command {
    std::string name;
    int index;
    bool mapped;
  };

  struct LogicalKey {
    std::string name;
    Key both;
    Key left;
    Key right;
  };

  using It = std::string::const_iterator;

  [[noreturn]] void error(std::string message) const;
  void parse_line(It begin, It end);
  void parse_context(It* begin, It end);
  KeySequence parse_modifier_list(std::string_view string);
  void parse_macro(std::string name, It begin, It end);
  bool parse_logical_key_definition(const std::string& name, It it, It end);
  void parse_mapping(const std::string& name, It begin, It end);
  std::string parse_command_name(It begin, It end) const;
  void parse_command_and_mapping(It in_begin, It in_end,
                                 It out_begin, It out_end);
  KeySequence parse_input(It begin, It end);
  KeySequence parse_output(It begin, It end);
  std::string preprocess_ident(std::string ident) const;
  std::string preprocess(It begin, It end) const;
  std::string preprocess(const std::string& string) const;
  Key add_logical_key(std::string name, Key left, Key right);
  void replace_logical_key(Key both, Key left, Key right);
  std::string read_filter_string(It* it, It end);
  Config::Filter read_filter(It* it, It end);
  Key get_key_by_name(std::string_view name) const;
  Key add_terminal_command_action(std::string_view command);
  void optimize_contexts();

  Config::Context& current_context();
  Command* find_command(const std::string& name);
  void add_command(KeySequence input, std::string name);
  void add_mapping(KeySequence input, KeySequence output);
  void add_mapping(const std::string& name, KeySequence output);

  int m_line_no{ };
  Config m_config;
  std::vector<Command> m_commands;
  std::map<std::string, std::string> m_macros;
  std::vector<LogicalKey> m_logical_keys;
  ParseKeySequence m_parse_sequence;
  bool m_system_filter_matched{ true };
  bool m_after_empty_context_block{ };
};
