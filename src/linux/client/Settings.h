#pragma once

#include <string>

struct Settings {
  std::string config_file_path;
  bool auto_update_config;
};

bool interpret_commandline(Settings& settings, int argc, char* argv[]);
void print_help_message(const char* arg0);
