#pragma once

#include "runtime/KeyEvent.h"
#include "common/Filter.h"

struct Config {
  struct Input {
    KeySequence input;
    // positive for direct-, negative for command output
    int output_index;
  };

  struct CommandOutput {
    KeySequence output;
    int index;
  };

  struct Context {
    bool system_filter_matched{ };
    Filter window_class_filter;
    Filter window_title_filter;
    Filter window_path_filter;
    Filter device_filter;
    Filter device_id_filter;
    KeySequence modifier_filter;
    std::vector<Input> inputs;
    std::vector<KeySequence> outputs;
    std::vector<CommandOutput> command_outputs;
    bool invert_modifier_filter{ };
    bool fallthrough{ };
    bool begin_stage{ };

    bool matches(const std::string& window_class,
                 const std::string& window_title,
                 const std::string& window_path) const {
      if (!window_class_filter.matches(window_class, false))
        return false;
      if (!window_title_filter.matches(window_title, true))
        return false;
      if (!window_path_filter.matches(window_path, true))
        return false;
      return true;
    }
  };

  struct Action {
    std::string terminal_command;
  };

  std::vector<Context> contexts;
  std::vector<Action> actions;
  std::vector<std::pair<std::string, Key>> virtual_key_aliases;
};
