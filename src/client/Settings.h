#pragma once

#include <filesystem>

extern std::filesystem::path default_config_filename;

struct Settings {
  std::filesystem::path config_file_path;
  bool auto_update_config;
  bool verbose;
  bool check_config;
  bool no_tray_icon;
  bool no_notify;
};

#if defined(_WIN32)
bool interpret_commandline(Settings& settings, int argc, wchar_t* argv[]);
#else
bool interpret_commandline(Settings& settings, int argc, char* argv[]);
#endif
void print_help_message();
