
#include "ClientPort.h"
#include "GrabbedKeyboards.h"
#include "uinput_keyboard.h"
#include "Settings.h"
#include "runtime/Stage.h"
#include "../common.h"
#include <linux/uinput.h>

namespace {
  const auto ipc_id = "keymapper";
  const auto uinput_keyboard_name = "Keymapper";
}

int main(int argc, char* argv[]) {
  auto settings = Settings{ };

  if (!interpret_commandline(settings, argc, argv)) {
    print_help_message(argv[0]);
    return 1;
  }
  g_verbose_output = settings.verbose;

  auto client = ClientPort();
  if (!client.initialize(ipc_id)) {
    error("Initializing keymapper connection failed");
    return 1;
  }

  // wait for client connection loop
  for (;;) {
    verbose("Waiting for keymapper to connect");
    auto stage = client.receive_config();
    if (stage) {
      // client connected
      verbose("Creating uinput keyboard '%s'", uinput_keyboard_name);
      const auto uinput_fd = create_uinput_keyboard(uinput_keyboard_name);
      if (uinput_fd < 0) {
        error("Creating uinput keyboard failed");
        return 1;
      }

      const auto grabbed_keyboards = grab_keyboards(uinput_keyboard_name);
      if (!grabbed_keyboards) {
        error("Initializing keyboard grabbing failed");
        return 1;
      }

      // main loop
      verbose("Entering update loop");
      auto output_buffer = KeySequence{ };
      for (;;) {
        // wait for next key event
        auto type = 0;
        auto code = 0;
        auto value = 0;
        if (!read_keyboard_event(*grabbed_keyboards, &type, &code, &value)) {
          verbose("Reading keyboard event failed");
          break;
        }

        if (type == EV_KEY) {
          // let client update configuration
          if (!stage->is_output_down())
            if (!client.receive_updates(stage)) {
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

          // translate key events
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
      }
      verbose("Destroying uinput keyboard");
      destroy_uinput_keyboard(uinput_fd);
    }
    client.disconnect();
    verbose("---------------");
  }
}
