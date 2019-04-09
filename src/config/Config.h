#pragma once

#include "runtime/KeyEvent.h"
#include <string>

struct Context {
  std::string window_class_filter;
  std::string window_title_filter;
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

    if (!context.window_class_filter.empty() &&
        window_class != context.window_class_filter)
      continue;

    if (!context.window_title_filter.empty() &&
        window_title.find(context.window_title_filter) == std::string::npos)
      continue;

    return static_cast<int>(i);
  }
  return -1;
}
