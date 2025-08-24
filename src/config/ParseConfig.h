#pragma once

#include "runtime/KeyEvent.h"
#include "Config.h"
#include "ParseKeySequence.h"
#include <iosfwd>
#include <filesystem>
#include <map>

class ParseConfig {
public:
  struct ConfigError : std::runtime_error {
    using std::runtime_error::runtime_error;
  };

  Config operator()(std::istream& is,
    const std::filesystem::path& base_path = { });

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
  void parse_file(std::istream& is, std::string filename = "");
  void parse_line(std::string& line);
  void parse_directive(It begin, It end);
  void parse_context(It begin, It end);
  void parse_mapping(It begin, It end);
  KeySequence parse_modifier_list(std::string_view string);
  void parse_macro(std::string name, It begin, It end);
  bool parse_logical_key_definition(const std::string& name, It it, It end);
  void parse_mapping(const std::string& name, It begin, It end);
  std::string parse_command_name(It begin, It end) const;
  void parse_command_and_mapping(It in_begin, It in_end,
                                 It out_begin, It out_end);
  KeySequence parse_input(It begin, It end);
  KeySequence parse_output(It begin, It end);
  std::vector<Key> parse_forward_modifiers_list(It* it, It end);
  std::string substitute_variables(std::string string) const;
  std::string preprocess(It begin, It end, bool apply_arguments = true) const;
  std::string preprocess(std::string expression) const;
  std::string apply_builtin_macro(const std::string& ident,
    const std::vector<std::string>& arguments) const;
  Key add_logical_key(std::string name, Key left, Key right);
  void replace_logical_key(Key both, Key left, Key right);
  std::string read_filter_string(It* it, It end);
  Filter read_filter(It* it, It end, bool invert);
  Key get_key_by_name(std::string_view name) const;
  Key add_action(Config::ActionType action_type, std::string_view value = "");
  void optimize_contexts();
  void prepend_forward_modifier_mappings();
  void suppress_forwarded_modifiers_in_outputs();

  Config::Context& current_context();
  Command* find_command(const std::string& name);
  void add_command(KeySequence input, std::string name);
  void add_mapping(KeySequence input, KeySequence output);
  void add_mapping(const std::string& name, KeySequence output);
  void add_toggle_active_context(KeySequence input);

  bool m_parsing_done{ };
  std::filesystem::path m_base_path;
  std::string m_filename;
  int m_include_level{ };
  mutable int m_preprocess_level{ };
  int m_line_no{ };
  Config m_config;
  std::vector<Command> m_commands;
  std::map<std::string, std::string> m_macros;
  std::vector<LogicalKey> m_logical_keys;
  ParseKeySequence m_parse_sequence;
  bool m_system_filter_matched{ true };
  bool m_after_empty_context_block{ };
  bool m_enforce_lowercase_commands{ };
  bool m_allow_unmapped_commands{ };
  std::vector<Key> m_forward_modifiers;
};
