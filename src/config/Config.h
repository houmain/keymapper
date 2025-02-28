#pragma once

#include "runtime/KeyEvent.h"
#include "common/Filter.h"
#include <filesystem>

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
    Filter window_class_filter;
    Filter window_title_filter;
    Filter window_path_filter;
    Filter device_filter;
    Filter device_id_filter;
    KeySequence modifier_filter;
    std::vector<Input> inputs;
    std::vector<KeySequence> outputs;
    std::vector<CommandOutput> command_outputs;
    bool system_filter_matched{ };
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

  enum class Option {
    auto_update_config,
    no_auto_update_config,
    verbose,
    no_tray_icon,
    no_notify,
  };

  std::vector<Context> contexts;
  std::vector<Action> actions;
  std::vector<std::pair<std::string, Key>> virtual_key_aliases;
  std::vector<GrabDeviceFilter> grab_device_filters;
  std::vector<std::string> server_directives;
  std::vector<std::filesystem::path> include_filenames;
  std::vector<Option> options;
};
