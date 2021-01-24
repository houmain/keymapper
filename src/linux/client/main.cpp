
#include "output.h"
#include "ipc.h"
#include "FocusedWindow.h"
#include "Settings.h"
#include "ConfigFile.h"
#include "config/Config.h"
#include <cstdarg>
#include <unistd.h>

namespace {
  const auto ipc_fifo_filename = "/tmp/keymapper";
  const auto config_filename = get_home_directory() + "/.config/keymapper.conf";
  const auto update_interval_ms = 50;
  bool g_verbose_output = false;
}

void error(const char* format, ...) {
  va_list args;
  va_start(args, format);
  std::vfprintf(stderr, format, args);
  va_end(args);
  std::fputc('\n', stderr);
}

void verbose(const char* format, ...) {
  if (g_verbose_output) {
    va_list args;
    va_start(args, format);
    std::vfprintf(stdout, format, args);
    va_end(args);
    std::fputc('\n', stdout);
    std::fflush(stdout);
  }
}

int main(int argc, char* argv[]) {
  auto settings = Settings{ };
  settings.config_file_path = config_filename;

  if (!interpret_commandline(settings, argc, argv)) {
    print_help_message(argv[0]);
    return 1;
  }
  g_verbose_output = settings.verbose;

  // load initial configuration
  verbose("loading configuration file '%s'", settings.config_file_path.c_str());
  auto config_file = ConfigFile(settings.config_file_path);
  if (!config_file.update()) {
    error("loading configuration failed");
    return 1;
  }
  for (;;) {
    // initialize client/server IPC
    verbose("connecting to keymapperd");
    const auto ipc_fd = initialize_ipc(ipc_fifo_filename);
    if (ipc_fd < 0)
      continue;

    // initialize focused window detection
    verbose("initializing focused window detection");
    auto focused_window = create_focused_window();
    if (!focused_window) {
      error("initializing focused window detection failed");
    }

    // send configuration
    verbose("sending configuration to keymapperd");
    if (send_config(ipc_fd, config_file.config())) {
      // main loop
      verbose("entering update loop");
      auto active_override_set = -1;
      for (;;) {
        // update configuration, reset on success
        if (settings.auto_update_config &&
            config_file.update()) {
          verbose("configuration updated");
          break;
        }

        if (is_pipe_broken((ipc_fd))) {
          verbose("connection to keymapperd lost");
          break;
        }

        // update active override set
        if (focused_window && update_focused_window(*focused_window)) {
          verbose("detected focused window changed:");
          verbose("  class = '%s'", get_class(*focused_window).c_str());
          verbose("  title = '%s'", get_title(*focused_window).c_str());

          const auto override_set = find_context(
            config_file.config(),
            get_class(*focused_window),
            get_title(*focused_window));

          if (active_override_set != override_set) {
            if (override_set >= 0) {
              verbose("sending 'active context #%i' to keymapperd:", override_set + 1);
              const auto& context = config_file.config().contexts[override_set];
              if (!context.window_class_filter.empty())
                verbose("  class filter = '%s'", context.window_class_filter.c_str());
              if (!context.window_title_filter.empty())
                verbose("  title filter = '%s'", context.window_title_filter.c_str());
            }
            else {
              verbose("sending 'no active context' to keymapperd");
            }

            active_override_set = override_set;
            if (!send_active_override_set(ipc_fd, active_override_set)) {
              verbose("connection to keymapperd lost");
              break;
            }
          }
        }
        usleep(update_interval_ms * 1000);
      }
    }
    shutdown_ipc(ipc_fd);
    verbose("---------------");
  }
}
