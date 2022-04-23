#pragma once

#include <filesystem>

struct Settings {
  std::filesystem::path config_file_path;
  bool auto_update_config;
  bool verbose;
  bool no_color;
  bool check_config;

#if defined(_WIN32)
  bool run_interception;
#endif
};

#if defined(_WIN32)
bool interpret_commandline(Settings& settings, int argc, wchar_t* argv[]);
#else
bool interpret_commandline(Settings& settings, int argc, char* argv[]);
#endif
void print_help_message();
