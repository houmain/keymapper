#pragma once

#include "runtime/KeyEvent.h"
#include <string>
#include <optional>
#include <regex>

struct Filter {
  std::string string;
  std::optional<std::regex> regex;

  explicit operator bool() const { return !string.empty(); }

  bool matches(const std::string& text, bool substring) const {
    if (string.empty())
      return true;
    if (regex.has_value())
      return std::regex_search(text, *regex);
    return (substring ?
      text.find(string) != std::string::npos :
      text == string);
  }
};

struct Context {
  bool system_filter_matched;
  Filter window_class_filter;
  Filter window_title_filter;
};

struct ContextMapping {
  int context_index;
  KeySequence output;
};

struct Command {
  std::string name;
  KeySequence input;
  KeySequence default_mapping;
  std::vector<ContextMapping> context_mappings;
};

struct Config {
  std::vector<Command> commands;
  std::vector<Context> contexts;
};

inline int find_context(const Config& config,
    const std::string& window_class,
    const std::string& window_title) {

  for (auto i = 0u; i < config.contexts.size(); ++i) {
    const auto& context = config.contexts[i];

    if (!context.window_class_filter.matches(window_class, false))
      continue;

    if (!context.window_title_filter.matches(window_title, true))
      continue;

    return static_cast<int>(i);
  }
  return -1;
}
