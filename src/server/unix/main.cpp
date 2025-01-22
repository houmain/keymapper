
#include "GrabbedDevices.h"
#include "VirtualDevices.h"
#include "server/Settings.h"
#include "server/ServerState.h"
#include "runtime/Timeout.h"
#include "common/output.h"
#include <csignal>
#include <atomic>

namespace {
  class ServerStateImpl final : public ServerState {
  private:
    bool on_send_key(const KeyEvent& event) override;
    void on_exit_requested() override;
    void on_configuration_message(MultiStagePtr stage) override;
    void on_grab_device_filters_message(
      std::vector<GrabDeviceFilter> filters) override;
    void on_directives_message(
      const std::vector<std::string>& directives) override;
  };
  
  VirtualDevices g_virtual_devices;
  GrabbedDevices g_grabbed_devices;
  int g_interrupt_fd;
  std::atomic<bool> g_shutdown;
  std::vector<GrabDeviceFilter> m_grab_device_filters;
  bool g_grab_device_filters_changed;
  ServerStateImpl g_state;
  
  bool ServerStateImpl::on_send_key(const KeyEvent& event) {
    return g_virtual_devices.send_key_event(event);
  }

  void ServerStateImpl::on_exit_requested() {
    g_shutdown.store(true);
  }

  void ServerStateImpl::on_configuration_message(MultiStagePtr stage) {
    if (has_configuration() &&
        has_mouse_mappings() != stage->has_mouse_mappings()) {
      verbose("Mouse usage in configuration changed");
      g_grab_device_filters_changed = true;
    }
    ServerState::on_configuration_message(std::move(stage));
  }
    
  void ServerStateImpl::on_grab_device_filters_message(
      std::vector<GrabDeviceFilter> filters) {
    const auto filters_changed =
      !std::equal(m_grab_device_filters.begin(),
        m_grab_device_filters.end(), filters.begin(), filters.end(),
        [](const GrabDeviceFilter& a, const GrabDeviceFilter& b) {
          return (std::tie(a.string, a.invert, a.by_id) ==
                  std::tie(b.string, b.invert, b.by_id));
        });

    if (has_configuration() && filters_changed) {
      verbose("Grab device filters changed");
      g_grab_device_filters_changed = true;
    }
    m_grab_device_filters = std::move(filters);
  }
  
  void ServerStateImpl::on_directives_message(
      const std::vector<std::string>& directives) {

#if defined(__APPLE__)
    macos_iso_keyboard = (std::count(begin(directives),
      end(directives), "macos-iso-keyboard") > 0);
    macos_toggle_fn = (std::count(begin(directives),
      end(directives), "macos-toggle-fn") > 0);
#endif

    ServerState::on_directives_message(directives);
  }

  bool read_initial_config() {
    while (!g_state.has_configuration()) {
      if (!g_state.read_client_messages()) {
        error("Receiving configuration failed");
        return false;
      }
    }
    return true;
  }

  bool main_loop() {
    auto& s = g_state;
    for (;;) {
      // wait for next input event
      auto now = Clock::now();
      auto timeout = std::optional<Duration>();
      const auto set_timeout = [&](const Duration& duration) {
        if (!timeout || duration < timeout)
          timeout = duration;
      };
      
      if (s.flush_scheduled_at())
        set_timeout(s.flush_scheduled_at().value() - now);
      if (s.timeout_start_at())
        set_timeout(s.timeout_start_at().value() + s.timeout() - now);

      if (g_shutdown.load()) {
        verbose("Received shutdown signal");
        return false;
      }

      // interrupt waiting when client sends an update
      const auto [succeeded, input] =
        g_grabbed_devices.read_input_event(timeout, g_interrupt_fd);
      if (!succeeded) {
        error("Reading input event failed");
        return true;
      }

      now = Clock::now();

      if (input) {
        if (auto event = to_key_event(input.value())) {
          if (event->key != Key::none)
            s.translate_input(event.value(), input->device_index);
        }
        else {
          // forward other events
          g_virtual_devices.forward_event(input->device_index,
            input->type, input->code, input->value);
          continue;
        }
      }

      if (s.timeout_start_at() &&
          now >= s.timeout_start_at().value() + s.timeout()) {
        const auto timeout = make_input_timeout_event(s.timeout());
        s.cancel_timeout();
        s.translate_input(timeout, Stage::any_device_index);
      }

      if (!s.flush_scheduled_at() || now > s.flush_scheduled_at()) {
        if (!s.flush_send_buffer()) {
          error("Sending input failed");
          return true;
        }
      }

      if (g_grabbed_devices.update_devices()) {
        if (!g_virtual_devices.update_forward_devices(
            g_grabbed_devices.grabbed_device_descs())) {
          verbose("Updating virtual forward devices failed");
          return true;
        }
        s.set_device_descs(g_grabbed_devices.grabbed_device_descs());
      }

      // let client update configuration and context
      if (g_interrupt_fd >= 0)
        if (!s.read_client_messages(Duration::zero()) ||
            std::exchange(g_grab_device_filters_changed, false) ||
            !s.has_configuration()) {
          verbose("Connection to keymapper reset");
          return true;
        }

      if (s.should_exit())
        return false;
    }
  }

  void handle_shutdown_signal(int) {
    g_shutdown.store(true);
    g_state.disconnect();
  }
 
  int connection_loop() {
    while (!g_shutdown.load()) {
      verbose("Waiting for keymapper to connect");
      const auto client_socket = g_state.accept_client_connection();

      if (g_state.version_mismatch()) {
        error("Client version mismatch detected");
        return 1;
      }
      if (!client_socket)
        continue;

      g_interrupt_fd = *client_socket;

      if (read_initial_config()) {
        if (!g_virtual_devices.create_keyboard_device()) {
          error("Creating virtual keyboard failed");
          return 1;
        }

        if (!g_grabbed_devices.grab(g_state.has_mouse_mappings(),
              m_grab_device_filters)) {
          error("Initializing input device grabbing failed");
          return 1;
        }
        if (!g_virtual_devices.update_forward_devices(
              g_grabbed_devices.grabbed_device_descs())) {
          error("Creating virtual forward devices failed");
          return 1;
        }
        g_state.set_device_descs(g_grabbed_devices.grabbed_device_descs());

        const auto prev_sigint_handler = ::signal(SIGINT, handle_shutdown_signal);
        const auto prev_sigterm_handler = ::signal(SIGTERM, handle_shutdown_signal);

        verbose("Entering update loop");
        if (!main_loop())
          g_shutdown.store(true);
        g_state.reset_configuration();

        ::signal(SIGINT, prev_sigint_handler);
        ::signal(SIGTERM, prev_sigterm_handler);
      }
      g_grabbed_devices = { };
      g_virtual_devices = { };
      g_state.disconnect();
      verbose("---------------");
    }
    return 0;
  }
} // namespace

int main(int argc, char* argv[]) {
  auto settings = Settings{ };

  if (!interpret_commandline(settings, argc, argv)) {
    print_help_message();
    return 1;
  }
  g_verbose_output = settings.verbose;

#if defined(__APPLE__)
  // when running as user in the graphical environment try to grab input device and exit.
  // it will fail but user is asked to grant permanent permission to monitor input.
  if (settings.grab_and_exit)
    return (g_grabbed_devices.grab(false, { }) ? 0 : 1);
#endif

  if (!g_state.listen_for_client_connections())
    return 1;

  return connection_loop();
}
