
#include "server/ClientPort.h"
#include "GrabbedDevices.h"
#include "uinput_device.h"
#include "server/Settings.h"
#include "runtime/Stage.h"
#include "common/output.h"
#include <linux/uinput.h>

namespace {
  const auto uinput_device_name = "Keymapper";

  bool read_client_messages(ClientPort& client,
      std::unique_ptr<Stage>& stage, int timeout) {
    return client.read_messages(timeout, [&](Deserializer& d) {
      const auto message_type = d.read<MessageType>();
      if (message_type == MessageType::configuration) {
        const auto prev_stage = std::move(stage);
        stage = client.read_config(d);
        verbose("Received configuration");

        if (prev_stage &&
            prev_stage->has_mouse_mappings() != stage->has_mouse_mappings()) {
          verbose("Mouse usage in configuration changed");
          stage.reset();
        }
      }
      else if (message_type == MessageType::active_contexts) {
        const auto& contexts = client.read_active_contexts(d);
        verbose("Received contexts (%d)", contexts.size());
        if (stage)
          stage->set_active_contexts(contexts);
      }
    });
  }

  std::unique_ptr<Stage> read_initial_config(ClientPort& client) {
    const auto no_timeout = -1;
    auto stage = std::unique_ptr<Stage>();
    while (!stage) {
      if (!read_client_messages(client, stage, no_timeout)) {
        error("Receiving configuration failed");
        return nullptr;
      }
    }
    return stage;
  }

  bool send_event(const KeyEvent& event, ClientPort& client, int uinput_fd) {
    if (is_action_key(event.key)) {
      if (event.state == KeyState::Down)
        return client.send_triggered_action(*event.key - *Key::first_action);
      return true;
    }
    return send_key_event(uinput_fd, event);
  }

  void translate_key_event(const KeyEvent& event, bool is_key_repeat,
      Stage& stage, ClientPort& client, int uinput_fd,
      KeySequence& output_buffer) {

    // after an OutputOnRelease event?
    if (!output_buffer.empty()) {
      // suppress key repeats
      if (is_key_repeat)
        return;

      // send rest of output buffer
      for (const auto& ev : output_buffer)
        if (ev.state != KeyState::OutputOnRelease)
          send_event(ev, client, uinput_fd);
    }

    // apply input
    stage.reuse_buffer(std::move(output_buffer));
    output_buffer = stage.update(event);

    auto it = output_buffer.begin();
    for (; it != output_buffer.end(); ++it) {
      // stop sending output on OutputOnRelease event
      if (it->state == KeyState::OutputOnRelease)
        break;
      send_event(*it, client, uinput_fd);
    }
    output_buffer.erase(output_buffer.begin(), it);
  }

  bool main_loop(ClientPort& client, std::unique_ptr<Stage>& stage,
      GrabbedDevices& grabbed_devices, int uinput_fd) {

    auto output_buffer = KeySequence{ };
    for (;;) {
      // wait for next input event
      auto type = 0;
      auto code = 0;
      auto value = 0;
      if (!read_input_event(grabbed_devices, &type, &code, &value)) {
        error("Reading input event failed");
        return true;
      }

      if (type != EV_KEY) {
        // forward other events
        ::send_event(uinput_fd, type, code, value);
        continue;
      }

      const auto event = KeyEvent{
        static_cast<Key>(code),
        (value == 0 ? KeyState::Up : KeyState::Down),
      };
      const auto is_key_repeat = (value == 2);
      translate_key_event(event, is_key_repeat,
        *stage, client, uinput_fd, output_buffer);

      // let client update configuration and context
      const auto timeout = 0;
      if (!stage->is_output_down() &&
          (!read_client_messages(client, stage, timeout) ||
           !stage)) {
        verbose("Connection to keymapper reset");
        return true;
      }

      if (stage->should_exit()) {
        verbose("Read exit sequence");
        return false;
      }
    }
  }

  int connection_loop(ClientPort& client) {
    for (;;) {
      verbose("Waiting for keymapper to connect");
      if (!client.accept()) {
        error("Accepting client connection failed");
        continue;
      }

      if (auto stage = read_initial_config(client)) {
        verbose("Creating uinput device '%s'", uinput_device_name);
        const auto uinput_fd = create_uinput_device(
          uinput_device_name, stage->has_mouse_mappings());
        if (uinput_fd < 0) {
          error("Creating uinput device failed");
          return 1;
        }

        const auto grabbed_devices = grab_devices(
          uinput_device_name, stage->has_mouse_mappings());
        if (!grabbed_devices) {
          error("Initializing input device grabbing failed");
          return 1;
        }

        verbose("Entering update loop");
        if (!main_loop(client, stage, *grabbed_devices, uinput_fd)) {
          verbose("Exiting");
          return 0;
        }

        verbose("Destroying uinput device");
        destroy_uinput_device(uinput_fd);
      }
      client.disconnect();
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

  auto client = ClientPort();
  if (!client.initialize()) {
    error("Initializing keymapper connection failed");
    return 1;
  }

  return connection_loop(client);
}
