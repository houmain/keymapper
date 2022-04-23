
#include "Settings.h"
#include "common/output.h"

#if defined(_WIN32)
bool interpret_commandline(Settings& settings, int argc, wchar_t* argv[]) {
#  define T(text) L##text
  for (auto i = 1; i < argc; i++) {
    const auto argument = std::wstring_view(argv[i]);
#else
bool interpret_commandline(Settings& settings, int argc, char* argv[]) {
#  define T(text) text
  for (auto i = 1; i < argc; i++) {
    const auto argument = std::string_view(argv[i]);
#endif

    if (argument == T("-v") || argument == T("--verbose")) {
      settings.verbose = true;
    }
    else {
      return false;
    }
  }
  return true;
}

void print_help_message() {
  const auto version =
#if __has_include("../../_version.h")
# include "../../_version.h"
  " ";
#else
  "";
#endif

  message(
    "keymapperd %s(c) 2019-%s by Albert Kalchmair\n"
    "\n"
    "Usage: keymapperd [-options]\n"
    "  -v, --verbose        enable verbose output.\n"
    "  -h, --help           print this help.\n"
    "\n"
    "All Rights Reserved.\n"
    "This program comes with absolutely no warranty.\n"
    "See the GNU General Public License, version 3 for details.\n"
    "\n", version, (__DATE__) + 7);
}
