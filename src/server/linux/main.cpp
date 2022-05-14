
#include "server/ClientPort.h"
#include "GrabbedDevices.h"
#include "UinputDevice.h"
#include "server/Settings.h"
#include "runtime/Stage.h"
#include "common/output.h"
#include <linux/uinput.h>

namespace {
  const auto uinput_device_name = "Keymapper";

  ClientPort g_client;
  std::unique_ptr<Stage> g_stage;
  UinputDevice g_uinput_device;
  GrabbedDevices g_grabbed_devices;

  void update_device_indices() {
    g_stage->set_device_indices(g_grabbed_devices.grabbed_device_names());
  }

  bool read_client_messages(int timeout_ms) {
    return g_client.read_messages(timeout_ms, [&](Deserializer& d) {
      const auto message_type = d.read<MessageType>();
      if (message_type == MessageType::configuration) {
        const auto prev_stage = std::move(g_stage);
        g_stage = g_client.read_config(d);
        verbose("Received configuration");

        if (prev_stage &&
            prev_stage->has_mouse_mappings() != g_stage->has_mouse_mappings()) {
          verbose("Mouse usage in configuration changed");
          g_stage.reset();
        }
        else {
          update_device_indices();
        }
      }
      else if (message_type == MessageType::active_contexts) {
        const auto& contexts = g_client.read_active_contexts(d);
        verbose("Received contexts (%d)", contexts.size());
        if (g_stage)
          g_stage->set_active_contexts(contexts);
      }
    });
  }

  bool read_initial_config() {
    const auto no_timeout = -1;
    while (!g_stage) {
      if (!read_client_messages(no_timeout)) {
        error("Receiving configuration failed");
        return false;
      }
    }
    return true;
  }

  bool send_event(const KeyEvent& event) {
    if (is_action_key(event.key)) {
      if (event.state == KeyState::Down)
        return g_client.send_triggered_action(*event.key - *Key::first_action);
      return true;
    }
    return g_uinput_device.send_key_event(event);
  }

  void translate_key_event(const KeyEvent& event, int device_index,
      bool is_key_repeat, KeySequence& output_buffer) {

    // after an OutputOnRelease event?
    if (!output_buffer.empty()) {
      // suppress key repeats
      if (is_key_repeat)
        return;

      // send rest of output buffer
      for (const auto& ev : output_buffer)
        if (ev.state != KeyState::OutputOnRelease)
          send_event(ev);
    }

    // apply input
    g_stage->reuse_buffer(std::move(output_buffer));
    output_buffer = g_stage->update(event, device_index);

    auto it = output_buffer.begin();
    for (; it != output_buffer.end(); ++it) {
      // stop sending output on OutputOnRelease event
      if (it->state == KeyState::OutputOnRelease)
        break;
      send_event(*it);
    }
    output_buffer.erase(output_buffer.begin(), it);
  }

  bool main_loop() {
    auto output_buffer = KeySequence{ };
    for (;;) {
      // wait for next input event
      // timeout, so updates from client do not pile up
      auto timeout_ms = 10;
      const auto [succeeded, input] =
        g_grabbed_devices.read_input_event(timeout_ms);
      if (!succeeded) {
        error("Reading input event failed");
        return true;
      }

      if (input) {
        if (input->type != EV_KEY) {
          // forward other events
          g_uinput_device.send_event(input->type, input->code, input->value);
          continue;
        }

        const auto event = KeyEvent{
          static_cast<Key>(input->code),
          (input->value == 0 ? KeyState::Up : KeyState::Down),
        };
        const auto is_key_repeat = (input->value == 2);
        translate_key_event(event, input->device_index,
          is_key_repeat, output_buffer);
      }

      // let client update configuration and context
      timeout_ms = 0;
      if (!g_stage->is_output_down())
        if (!read_client_messages(timeout_ms) ||
            !g_stage) {
          verbose("Connection to keymapper reset");
          return true;
        }

      if (g_stage->should_exit()) {
        verbose("Read exit sequence");
        return false;
      }
    }
  }

  int connection_loop() {
    for (;;) {
      verbose("Waiting for keymapper to connect");
      if (!g_client.accept()) {
        error("Accepting client connection failed");
        continue;
      }

      if (read_initial_config()) {
        verbose("Creating uinput device '%s'", uinput_device_name);
        if (!g_uinput_device.create(uinput_device_name,
            g_stage->has_mouse_mappings())) {
          error("Creating uinput device failed");
          return 1;
        }

        if (!g_grabbed_devices.grab(uinput_device_name,
              g_stage->has_mouse_mappings())) {
          error("Initializing input device grabbing failed");
          g_uinput_device = { };
          return 1;
        }

        update_device_indices();

        verbose("Entering update loop");
        if (!main_loop()) {
          verbose("Exiting");
          return 0;
        }
      }
      g_grabbed_devices = { };
      g_uinput_device = { };
      g_client.disconnect();
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

  if (!g_client.initialize()) {
    error("Initializing keymapper connection failed");
    return 1;
  }

  return connection_loop();
}
