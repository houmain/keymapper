#pragma once

#include <string>

struct Settings {
  std::wstring config_file_path;
  bool auto_update_config;
  bool run_interception;
  bool verbose;
};

bool interpret_commandline(Settings& settings, int argc, wchar_t* argv[]);
void print_help_message(const wchar_t* argv0);
