
#include "server/ClientPort.h"
#include "GrabbedDevices.h"
#include "uinput_device.h"
#include "server/Settings.h"
#include "runtime/Stage.h"
#include "common/output.h"
#include <linux/uinput.h>

namespace {
  const auto uinput_device_name = "Keymapper";
}

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

  // wait for client connection loop
  auto read_exit_sequence = false;
  while (!read_exit_sequence) {
    verbose("Waiting for keymapper to connect");
    auto stage = std::unique_ptr<Stage>();
    if (client.accept() &&
        client.read_messages(-1, [&](Deserializer& d) {
          if (d.read<MessageType>() == MessageType::configuration)
            stage = client.read_config(d);
        }) &&
        stage) {
      // client connected
      verbose("Creating uinput device '%s'", uinput_device_name);
      const auto uinput_fd = create_uinput_device(uinput_device_name);
      if (uinput_fd < 0) {
        error("Creating uinput device failed");
        return 1;
      }

      const auto grabbed_devices = grab_devices(uinput_device_name);
      if (!grabbed_devices) {
        error("Initializing input device grabbing failed");
        return 1;
      }

      // main loop
      verbose("Entering update loop");
      auto output_buffer = KeySequence{ };
      while (!read_exit_sequence) {
        // wait for next input event
        auto type = 0;
        auto code = 0;
        auto value = 0;
        if (!read_input_event(*grabbed_devices, &type, &code, &value)) {
          verbose("Reading input event failed");
          break;
        }

        if (type == EV_KEY) {
          // let client update configuration
          if (!stage->is_output_down() &&
              !client.read_messages(0,
                [&](Deserializer& d) {
                  const auto message_type = d.read<MessageType>();
                  if (message_type == MessageType::configuration) {
                    stage = client.read_config(d);
                  }
                  else if (message_type == MessageType::active_contexts) {
                    stage->set_active_contexts(client.read_active_contexts(d));
                  }
                })) {
            verbose("Connection to keymapper reset");
            break;
          }

          const auto send_event = [&](const KeyEvent& event) {
            if (!is_action_key(event.key)) {
              send_key_event(uinput_fd, event);
            }
            else if (event.state == KeyState::Down) {
              client.send_triggered_action(event.key - first_action_key);
            }
          };

          // translate input events
          const auto event = KeyEvent{
            static_cast<KeyCode>(code),
            (value == 0 ? KeyState::Up : KeyState::Down),
          };

          // after an OutputOnRelease event?
          if (!output_buffer.empty()) {
            // suppress key repeats
            if (value == 2)
              continue;

            // send rest of output buffer
            for (const auto& event : output_buffer)
              if (event.state != KeyState::OutputOnRelease)
                send_event(event);
          }

          // apply input
          stage->reuse_buffer(std::move(output_buffer));
          output_buffer = stage->update(event);

          auto it = output_buffer.begin();
          for (; it != output_buffer.end(); ++it) {
            // stop sending output on OutputOnRelease event
            if (it->state == KeyState::OutputOnRelease)
              break;
            send_event(*it);
          }
          flush_events(uinput_fd);
          output_buffer.erase(output_buffer.begin(), it);
        }
        else {
          // forward other events
          send_event(uinput_fd, type, code, value);
        }

        if (stage->should_exit()) {
          verbose("Read exit sequence");
          read_exit_sequence = true;
        }
      }
      verbose("Destroying uinput device");
      destroy_uinput_device(uinput_fd);
    }
    client.disconnect();
    verbose("---------------");
  }
  verbose("Exiting");
}
