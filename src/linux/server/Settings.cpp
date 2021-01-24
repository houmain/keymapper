
#include "Settings.h"
#include <cstdio>

bool interpret_commandline(Settings& settings, int argc, char* argv[]) {
  for (auto i = 1; i < argc; i++) {
    const auto argument = std::string(argv[i]);
    if (argument == "-v" || argument == "--verbose") {
      settings.verbose = true;
    }
    else {
      return false;
    }
  }
  return true;
}

void print_help_message(const char* argv0) {
  auto program = std::string(argv0);
  if (auto i = program.rfind('/'); i != std::string::npos)
    program = program.substr(i + 1);
  if (auto i = program.rfind('.'); i != std::string::npos)
    program = program.substr(0, i);

  const auto version =
#if __has_include("../../_version.h")
# include "../../_version.h"
  " ";
#else
  "";
#endif

  std::printf(
    "keymapperd %s(c) 2019-2021 by Albert Kalchmair\n"
    "\n"
    "Usage: %s [-options]\n"
    "  -v, --verbose        enable verbose output.\n"
    "  -h, --help           print this help.\n"
    "\n"
    "All Rights Reserved.\n"
    "This program comes with absolutely no warranty.\n"
    "See the GNU General Public License, version 3 for details.\n"
    "\n", version, program.c_str());
}
