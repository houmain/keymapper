#pragma once

#include <string>

struct Settings {
  bool verbose;
};

bool interpret_commandline(Settings& settings, int argc, char* argv[]);
void print_help_message(const char* arg0);
