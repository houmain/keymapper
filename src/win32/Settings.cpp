
#include "Settings.h"
#include "common.h"

bool interpret_commandline(Settings& settings, int argc, wchar_t* argv[]) {
  for (auto i = 1; i < argc; i++) {
    const auto argument = std::wstring(argv[i]);
    if (argument == L"-u" || argument == L"--update") {
      settings.auto_update_config = true;
    }
    else if (argument == L"-c" || argument == L"--config") {
      if (++i >= argc)
        return false;
      settings.config_file_path = argv[i];
    }
    else if (argument == L"-i" || argument == L"--interception") {
      settings.run_interception = true;
    }
    else {
      return false;
    }
  }
  return true;
}

void print_help_message(const wchar_t* argv0) {
  const auto version = std::string(
#include "../_version.h"
  );
  print(("keymapper " + version + " (c) 2019 by Albert Kalchmair\n"
    "\n"
    "Usage: keymapper [-options]\n"
    "  -c, --config <path>  configuration file.\n"
    "  -u, --update         reload configuration file when it changes.\n"
    "  -i, --interception   use interception.\n"
    "\n"
    "All Rights Reserved.\n"
    "This program comes with absolutely no warranty.\n"
    "See the GNU General Public License, version 3 for details.\n"
    "\n").c_str());
}
