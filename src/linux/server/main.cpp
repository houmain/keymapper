
#include "ipc.h"
#include "keyboard.h"
#include "uinput_keyboard.h"
#include "runtime/Stage.h"
#include <linux/uinput.h>

namespace {
  const auto ipc_fifo_filename = "/tmp/keymapper";
  const auto uinput_keyboard_name = "Keymapper";
}

int main() {
  for (;;) {
    const auto ipc_fd = initialize_ipc(ipc_fifo_filename);
    if (ipc_fd < 0)
      return 1;

    const auto stage = read_config(ipc_fd);
    if (stage) {
      const auto event_fd = grab_first_keyboard();
      if (event_fd >= 0) {
        const auto uinput_fd = create_uinput_keyboard(uinput_keyboard_name);
        if (uinput_fd >= 0) {
          // main loop
          for (;;) {
            // let client update configuration
            if (!stage->is_output_down())
              if (!update_ipc(ipc_fd, *stage))
                break;

            // wait for next key event
            auto type = 0;
            auto code = 0;
            auto value = 0;
            if (!read_event(event_fd, &type, &code, &value))
              break;

            if (type == EV_KEY) {
              // translate key events
              const auto event = KeyEvent{
                static_cast<Key>(code),
                (value == 0 ? KeyState::Up : KeyState::Down),
              };
              auto output = stage->apply_input(event);
              send_key_sequence(uinput_fd, output);
              stage->reuse_buffer(std::move(output));
            }
            else if (type != EV_SYN &&
                     type != EV_MSC) {
              // forward other events
              send_event(uinput_fd, type, code, value);
            }
          }
          destroy_uinput_keyboard(uinput_fd);
        }
        release_keyboard(event_fd);
      }
    }
    shutdown_ipc(ipc_fd);
  }
}
