
#include "ServerPort.h"
#include "FocusedWindow.h"
#include "Settings.h"
#include "ConfigFile.h"
#include "config/Config.h"
#include "../common.h"
#include <csignal>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

namespace {
  const auto ipc_id = "keymapper";
  const auto config_filename = get_home_directory() + "/.config/keymapper.conf";
  const auto update_interval_ms = 50;

  void catch_child([[maybe_unused]] int sig_num) {
    auto child_status = 0;
    ::wait(&child_status);
  }

  void execute_terminal_command(const std::string& command) {
    verbose("Executing terminal command '%s'", command.c_str());
    if (fork() == 0) {
      dup2(open("/dev/null", O_RDONLY), STDIN_FILENO);
      if (!g_verbose_output) {
        dup2(open("/dev/null", O_RDWR), STDOUT_FILENO);
        dup2(open("/dev/null", O_RDWR), STDERR_FILENO);
      }
      execl("/bin/sh", "sh", "-c", command.c_str(), nullptr);
      exit(1);
    }
  }
} // namespace

int main(int argc, char* argv[]) {
  auto settings = Settings{ };
  settings.config_file_path = config_filename;

  if (!interpret_commandline(settings, argc, argv)) {
    print_help_message(argv[0]);
    return 1;
  }
  g_verbose_output = settings.verbose;
  g_output_color = settings.color;

  ::signal(SIGCHLD, &catch_child);

  // load initial configuration
  verbose("Loading configuration file '%s'", settings.config_file_path.c_str());
  auto config_file = ConfigFile(settings.config_file_path);
  if (!config_file.update()) {
    error("Loading configuration failed");
    return 1;
  }
  for (;;) {
    // initialize client/server IPC
    verbose("Connecting to keymapperd");
    auto server = ServerPort();
    if (!server.initialize(ipc_id) ||
        !server.send_config(config_file.config())) {
      error("Connecting to keymapperd failed");
      return 1;
    }

    // initialize focused window detection
    verbose("Initializing focused window detection");
    auto focused_window = create_focused_window();
    if (!focused_window) {
      error("Initializing focused window detection failed");
    }

    // main loop
    verbose("Entering update loop");
    auto active_override_set = -1;
    for (;;) {
      // update configuration, reset on success
      if (settings.auto_update_config &&
          config_file.update()) {
        verbose("Configuration updated");
        break;
      }

      // update active override set
      if (focused_window && update_focused_window(*focused_window)) {
        verbose("Detected focused window changed:");
        verbose("  class = '%s'", get_class(*focused_window).c_str());
        verbose("  title = '%s'", get_title(*focused_window).c_str());

        const auto override_set = find_context(
          config_file.config(),
          get_class(*focused_window),
          get_title(*focused_window));

        if (active_override_set != override_set) {
          if (override_set >= 0) {
            verbose("Sending 'active context #%i' to keymapperd:", override_set + 1);
            const auto& context = config_file.config().contexts[override_set];
            if (const auto& filter = context.window_class_filter)
              verbose("  class filter = '%s'", filter.string.c_str());
            if (const auto& filter = context.window_title_filter)
              verbose("  title filter = '%s'", filter.string.c_str());
          }
          else {
            verbose("Sending 'no active context' to keymapperd");
          }

          active_override_set = override_set;
          if (!server.send_active_override_set(active_override_set)) {
            verbose("Connection to keymapperd lost");
            break;
          }
        }
      }

      // receive triggered actions
      auto triggered_action = -1;
      if (!server.receive_triggered_action(update_interval_ms, &triggered_action)) {
        verbose("Connection to keymapperd lost");
        break;
      }
      if (triggered_action >= 0 &&
          triggered_action < static_cast<int>(config_file.config().actions.size())) {
        const auto& action = config_file.config().actions[triggered_action];
        execute_terminal_command(action.terminal_command);
      }
    }
    verbose("---------------");
  }
}
