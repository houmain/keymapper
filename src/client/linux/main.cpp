
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
#include <pwd.h>

namespace {
  const auto system_config_path = std::filesystem::path("/etc/");
  const auto update_interval = std::chrono::milliseconds(50);

  Settings g_settings;
  ServerPort g_server;
  ConfigFile g_config_file;
  FocusedWindow g_focused_window;
  std::vector<int> g_active_contexts;

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

  bool send_active_contexts() {
    g_active_contexts.clear();
    const auto& contexts = g_config_file.config().contexts;
    const auto& window_class = g_focused_window.window_class();
    const auto& window_title = g_focused_window.window_title();
    for (auto i = 0; i < static_cast<int>(contexts.size()); ++i)
      if (contexts[i].matches(window_class, window_title))
        g_active_contexts.push_back(i);

    return g_server.send_active_contexts(g_active_contexts);
  }

  bool receive_triggered_action() {
    auto triggered_action = -1;
    if (!g_server.receive_triggered_action(update_interval, &triggered_action))
      return false;

    const auto& actions = g_config_file.config().actions;
    if (triggered_action < 0)
      return true;

    if (triggered_action >= static_cast<int>(actions.size()))
      return false;

    execute_terminal_command(actions[triggered_action].terminal_command);
    return true;
  }

  void main_loop() {

    for (;;) {
      // update configuration
      auto configuration_updated = false;
      if (g_settings.auto_update_config &&
          g_config_file.update()) {
        message("Configuration updated");
        if (!g_server.send_config(g_config_file.config()))
          return;
        configuration_updated = true;
      }

      // update active override set
      if (g_focused_window.update() || configuration_updated) {
        verbose("Detected focused window changed:");
        verbose("  class = '%s'", g_focused_window.window_class().c_str());
        verbose("  title = '%s'", g_focused_window.window_title().c_str());
        if (!send_active_contexts())
          return;
      }

      if (!receive_triggered_action())
        return;
    }
  }

  int connection_loop() {
    for (;;) {
      verbose("Connecting to keymapperd");
      if (!g_server.initialize()) {
        error("Connecting to keymapperd failed");
        return 1;
      }

      verbose("Sending configuration");
      if (!g_server.send_config(g_config_file.config()) ||
          !send_active_contexts()) {
        error("Sending configuration failed");
        return 1;
      }

      verbose("Initializing focused window detection:");
      g_focused_window.initialize();

      verbose("Entering update loop");
      main_loop();
      verbose("Connection to keymapperd lost");

      g_focused_window.shutdown();

      verbose("---------------");
    }
  }

  std::filesystem::path get_home_path() {
    if (auto homedir = ::getenv("HOME"))
      return homedir;
    return ::getpwuid(::getuid())->pw_dir;
  }

  std::filesystem::path resolve_config_file_path(
      std::filesystem::path filename) {
    auto error = std::error_code{ };
    if (filename.empty()) {
      filename = default_config_filename;
      for (const auto& base : {
          get_home_path() / ".config",
          system_config_path
        }) {
        auto path = base / filename;
        if (std::filesystem::exists(path, error))
          return path;
      }
    }
    return std::filesystem::absolute(filename, error);
  }
} // namespace

void show_notification(const char* message) {
  auto ss = std::stringstream();
  ss << "notify-send -a keymapper keymapper \"" << message << "\"";
  std::system(ss.str().c_str());
}

int main(int argc, char* argv[]) {
  if (!interpret_commandline(g_settings, argc, argv)) {
    print_help_message();
    return 1;
  }
  g_verbose_output = g_settings.verbose;

  g_settings.config_file_path = 
    resolve_config_file_path(std::move(g_settings.config_file_path));

  ::signal(SIGCHLD, &catch_child);

  verbose("Loading configuration file '%s'", g_settings.config_file_path.c_str());
  if (!g_config_file.load(g_settings.config_file_path))
    return 1;

  if (g_settings.check_config) {
    message("The configuration is valid");
    return 0;
  }
  return connection_loop();
}
