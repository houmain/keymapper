
#include "client/FocusedWindow.h"
#include "client/ServerPort.h"
#include "client/Settings.h"
#include "client/ConfigFile.h"
#include "config/Config.h"
#include "common/output.h"
#include <csignal>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

namespace {
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

  bool send_active_contexts(ServerPort& server, const Config& config,
      const std::string& window_class, const std::string& window_title) {

    static std::vector<int> s_active_contexts;

    s_active_contexts.clear();
    for (auto i = 0; i < static_cast<int>(config.contexts.size()); ++i)
      if (config.contexts[i].matches(window_class, window_title))
        s_active_contexts.push_back(i);

    return server.send_active_contexts(s_active_contexts);
  }

  void main_loop(const Settings& settings, ServerPort& server,
      ConfigFile& config_file, FocusedWindow& focused_window) {

    for (;;) {
      // update configuration
      auto configuration_updated = false;
      if (settings.auto_update_config &&
          config_file.update()) {
        verbose("Configuration updated");
        if (!server.send_config(config_file.config()))
          return;
        configuration_updated = true;
      }

      // update active override set
      if (focused_window.update() || configuration_updated) {
        verbose("Detected focused window changed:");
        verbose("  class = '%s'", focused_window.window_class().c_str());
        verbose("  title = '%s'", focused_window.window_title().c_str());
        if (!send_active_contexts(server, config_file.config(),
              focused_window.window_class(), focused_window.window_title()))
          return;
      }

      // receive triggered actions
      auto triggered_action = -1;
      if (!server.receive_triggered_action(update_interval_ms, &triggered_action))
        return;

      if (triggered_action >= 0 &&
          triggered_action < static_cast<int>(config_file.config().actions.size())) {
        const auto& action = config_file.config().actions[triggered_action];
        execute_terminal_command(action.terminal_command);
      }
    }
  }

  int connection_loop(const Settings& settings, ConfigFile& config_file) {
    for (;;) {
      verbose("Connecting to keymapperd");
      auto server = ServerPort();
      if (!server.initialize()) {
        error("Connecting to keymapperd failed");
        return 1;
      }

      verbose("Sending configuration");
      if (!server.send_config(config_file.config()) ||
          !send_active_contexts(server, config_file.config(), "", "")) {
        error("Sending configuration failed");
        return 1;
      }

      verbose("Initializing focused window detection");
      auto focused_window = FocusedWindow();

      verbose("Entering update loop");
      main_loop(settings, server, config_file, focused_window);
      verbose("Connection to keymapperd lost");

      verbose("---------------");
    }
  }
} // namespace

int main(int argc, char* argv[]) {
  auto settings = Settings{ };

  if (!interpret_commandline(settings, argc, argv)) {
    print_help_message();
    return 1;
  }
  g_verbose_output = settings.verbose;

  ::signal(SIGCHLD, &catch_child);

  verbose("Loading configuration file '%s'", settings.config_file_path.c_str());
  auto config_file = ConfigFile(settings.config_file_path);
  if (!config_file.update()) {
    error("Loading configuration failed");
    return 1;
  }
  if (settings.check_config) {
    message("The configuration is valid");
    return 0;
  }
  return connection_loop(settings, config_file);
}
