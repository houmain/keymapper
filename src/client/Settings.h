#pragma once

#include <filesystem>
#include "config/Config.h"

extern std::filesystem::path default_config_filename;

struct Settings {
  std::filesystem::path config_file_path;
  bool auto_update_config{ };
  bool verbose{ };
  bool check_config{ };
  bool no_tray_icon{ };
  bool no_notify{ };
};

#if defined(_WIN32)
bool interpret_commandline(Settings& settings, int argc, wchar_t* argv[]);
#else
bool interpret_commandline(Settings& settings, int argc, char* argv[]);
#endif
void print_help_message();

inline Settings apply_config_options(
    const Settings& settings, const Config& config) {
  auto s = settings;
  using Option = Config::Option;
  for (auto option : config.options) {
    switch (option) {
      case Option::auto_update_config: s.auto_update_config = true; break;
      case Option::no_auto_update_config: s.auto_update_config = false; break;
      case Option::verbose: s.verbose = true; break;
      case Option::no_tray_icon: s.no_tray_icon = true; break;
      case Option::no_notify: s.no_notify = true; break;
    }
  }
  return s;
}
