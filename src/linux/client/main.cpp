
#include "ipc.h"
#include "FocusedWindow.h"
#include "Settings.h"
#include "ConfigFile.h"
#include <string>
#include <unistd.h>

namespace {
  const auto ipc_fifo_filename = "/tmp/keymapper";
  const auto config_filename = get_home_directory() + "/.config/keymapper.conf";
  const auto update_interval_ms = 50;
}

int main(int argc, char* argv[]) {
  auto settings = Settings{ };
  settings.config_file_path = config_filename;

  if (!interpret_commandline(settings, argc, argv)) {
    print_help_message(argv[0]);
    return 1;
  }
  // load initial configuration
  auto config_file = ConfigFile(settings.config_file_path);
  if (!config_file.update()) {
    std::fprintf(stderr, "loading configuration failed\n");
    return 1;
  }
  for (;;) {
    // initialize client/server IPC
    const auto ipc_fd = initialize_ipc(ipc_fifo_filename);
    if (ipc_fd < 0) {
      ::sleep(1);
      continue;
    }
    // initialize focused window detection
    auto focused_window = create_focused_window();

    // send configuration
    if (send_config(ipc_fd, config_file.config())) {
      // main loop
      auto active_override_set = -1;
      for (;;) {
        // update configuration, reset on success
        if (settings.auto_update_config &&
            config_file.update())
          break;

        // update active override set
        if (focused_window && update_focused_window(*focused_window)) {
          const auto override_set = find_context(
            config_file.config(),
            get_class(*focused_window),
            get_title(*focused_window));

          if (active_override_set != override_set) {
            active_override_set = override_set;
            if (!send_active_override_set(ipc_fd, active_override_set))
              break;
          }
        }
        usleep(update_interval_ms * 1000);
      }
    }
    shutdown_ipc(ipc_fd);
  }
}
