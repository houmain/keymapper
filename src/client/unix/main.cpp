
#include "client/Settings.h"
#include "client/ClientState.h"
#include "common/output.h"
#include <sstream>
#include <csignal>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <pwd.h>

namespace {
  const auto system_config_path = std::filesystem::path("/etc/");
  const auto update_interval = std::chrono::milliseconds(50);

  Settings g_settings;
  ClientState g_state;

  void catch_child([[maybe_unused]] int sig_num) {
    auto child_status = 0;
    ::wait(&child_status);
  }

  void main_loop() {
    for (;;) {
      // update configuration
      auto configuration_updated = false;
      if (g_settings.auto_update_config &&
          g_state.update_config(true)) {
        if (!g_state.send_config())
          return;

        configuration_updated = true;
      }

      if (g_state.update_active_contexts() || configuration_updated)
        if (!g_state.send_active_contexts())
          return;

      if (!g_state.read_server_messages(update_interval))
        return;

      g_state.accept_control_connection();
      g_state.read_control_messages();
    }
  }

  int connection_loop() {
    for (;;) {
      if (!g_state.connect_server())
        return 1;

      if (!g_state.send_config())
        return 1;

      if (!g_state.initialize_contexts())
        return 1;

      g_state.listen_for_control_connections();

      verbose("Entering update loop");
      main_loop();
      verbose("Connection to keymapperd lost");

      g_state.on_server_disconnected();

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

  void show_notification(const char* message) {
    auto ss = std::stringstream();
#if defined(__APPLE__)
    ss << "osascript -e 'display notification \"" << message << "\" with title \"keymapper\"'";
#else
    ss << "notify-send -a keymapper keymapper \"" << message << "\"";
#endif
    [[maybe_unused]] auto result = std::system(ss.str().c_str());
  }
} // namespace

bool execute_terminal_command(const std::string& command) {
  if (fork() == 0) {
    dup2(open("/dev/null", O_RDONLY), STDIN_FILENO);
    if (!g_verbose_output) {
      dup2(open("/dev/null", O_RDWR), STDOUT_FILENO);
      dup2(open("/dev/null", O_RDWR), STDERR_FILENO);
    }
    execl("/bin/sh", "sh", "-c", command.c_str(), nullptr);
    exit(1);
  }
  return true;
}

int main(int argc, char* argv[]) {
  if (!interpret_commandline(g_settings, argc, argv)) {
    print_help_message();
    return 1;
  }
  g_verbose_output = g_settings.verbose;
  if (!g_settings.no_tray_icon)
    g_show_notification = &show_notification;

  g_settings.config_file_path = 
    resolve_config_file_path(std::move(g_settings.config_file_path));

  ::signal(SIGCHLD, &catch_child);

  verbose("Loading configuration file '%s'", g_settings.config_file_path.c_str());
  if (!g_state.load_config(g_settings.config_file_path))
    return 1;

  if (g_settings.check_config) {
    message("The configuration is valid");
    return 0;
  }
  return connection_loop();
}
