#pragma once

#include <string>

struct Settings {
  bool verbose;
  bool grab_and_exit;
};

#if defined(_WIN32)
bool interpret_commandline(Settings& settings, int argc, wchar_t* argv[]);
#else
bool interpret_commandline(Settings& settings, int argc, char* argv[]);
#endif
void print_help_message();
