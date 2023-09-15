
#include "server/ClientPort.h"
#include "GrabbedDevices.h"
#include "UinputDevice.h"
#include "server/ButtonDebouncer.h"
#include "server/Settings.h"
#include "server/verbose_debug_io.h"
#include "runtime/Stage.h"
#include "runtime/Timeout.h"
#include "common/output.h"
#include <linux/uinput.h>
#include <map>

namespace {
  const auto uinput_device_name = "Keymapper";

  ClientPort g_client;
  std::unique_ptr<Stage> g_stage;
  UinputDevice g_uinput_device;
  GrabbedDevices g_grabbed_devices;
  std::optional<ButtonDebouncer> g_button_debouncer;
  std::map<Key, std::vector<KeyEvent>> g_send_buffer_on_release_map;
  std::vector<KeyEvent> g_send_buffer;
  std::optional<Clock::time_point> g_flush_scheduled_at;
  std::optional<Clock::time_point> g_input_timeout_start;
  std::chrono::milliseconds g_input_timeout;
  KeyEvent g_last_key_event;
  int g_last_device_index;

  void evaluate_device_filters() {
    g_stage->evaluate_device_filters(g_grabbed_devices.grabbed_device_names());
  }

  bool read_client_messages(std::optional<Duration> timeout = { }) {
    return g_client.read_messages(timeout, [&](Deserializer& d) {
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
          evaluate_device_filters();
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
    while (!g_stage) {
      if (!read_client_messages()) {
        error("Receiving configuration failed");
        return false;
      }
    }
    return true;
  }

  void schedule_flush(Duration delay) {
    if (g_flush_scheduled_at)
      return;
    g_flush_scheduled_at = Clock::now() +
      std::chrono::duration_cast<Clock::duration>(delay);
  }

  bool flush_send_buffer() {
    auto i = 0;
    for (; i < g_send_buffer.size(); ++i) {
      const auto& event = g_send_buffer[i];
      const auto is_last = (i == g_send_buffer.size() - 1);

      if (is_action_key(event.key)) {
        if (event.state == KeyState::Down)
          g_client.send_triggered_action(
            static_cast<int>(*event.key - *Key::first_action));
        continue;
      }

      if (event.key == Key::timeout) {
        schedule_flush(timeout_to_milliseconds(event.timeout));
        ++i;
        break;
      }

      if (g_button_debouncer &&
          event.state == KeyState::Down) {
        const auto delay = g_button_debouncer->on_key_down(event.key, !is_last);
        if (delay.count() > 0) {
          schedule_flush(delay);
          break;
        }
      }
      if (!g_uinput_device.send_key_event(event))
        return false;
    }
    g_send_buffer.erase(g_send_buffer.begin(), g_send_buffer.begin() + i);
    return true;
  }

  void send_key_sequence(const KeySequence& key_sequence) {
    auto* send_buffer = &g_send_buffer;
    for (const auto& event : key_sequence) {
      if (event.state == KeyState::OutputOnRelease) {
        auto output_buffer = std::vector<KeyEvent>();
        g_send_buffer_on_release_map.insert({g_last_key_event.key, output_buffer});
        send_buffer = &g_send_buffer_on_release_map[g_last_key_event.key];
      }
      else {
        send_buffer->push_back(event);
      }
    }
  }

  void translate_input(const KeyEvent& input, int device_index) {
    // ignore key repeat while a flush or a timeout is pending
    if (input == g_last_key_event &&
        (g_flush_scheduled_at || g_input_timeout_start))
      return;

    // cancel timeout when key is released/another is pressed
    if (g_input_timeout_start) {
      const auto time_since_timeout_start = 
        (Clock::now() - *g_input_timeout_start);
      g_input_timeout_start.reset();
      translate_input(make_input_timeout_event(time_since_timeout_start),
        device_index);
    }

    g_last_key_event = input;
    g_last_device_index = device_index;

    // after OutputOnRelease block input until trigger is released
    bool keyHasOutputOnRelease = g_send_buffer_on_release_map.count(input.key) != 0;
    if (keyHasOutputOnRelease) {
      if (input.state != KeyState::Up)
        return;
      auto send_buffer = g_send_buffer_on_release_map[input.key];
      g_send_buffer.insert(g_send_buffer.end(),
        send_buffer.begin(), send_buffer.end());
      send_buffer.clear();
      g_send_buffer_on_release_map.erase(input.key);
    }

    auto output = g_stage->update(input, device_index);

    verbose_debug_io(input, output, true);

    // waiting for input timeout
    if (!output.empty() && is_input_timeout_event(output.back())) {
      g_input_timeout_start = Clock::now();
      g_input_timeout = timeout_to_milliseconds(output.back().timeout);
      output.pop_back();
    }

    send_key_sequence(output);

    g_stage->reuse_buffer(std::move(output));
  }

  bool main_loop() {
    for (;;) {
      // wait for next input event
      auto now = Clock::now();
      auto timeout = std::optional<Duration>();
      const auto set_timeout = [&](const Duration& duration) {
        if (!timeout || duration < timeout)
          timeout = duration;
      };
      if (g_flush_scheduled_at)
        set_timeout(g_flush_scheduled_at.value() - now);
      if (g_input_timeout_start)
        set_timeout(g_input_timeout_start.value() + g_input_timeout - now);

      // interrupt waiting when no key is down and client sends an update
      const auto interrupt_fd = (!g_stage->is_output_down() ? g_client.socket() : -1);

      const auto [succeeded, input] =
        g_grabbed_devices.read_input_event(timeout, interrupt_fd);
      if (!succeeded) {
        error("Reading input event failed");
        return true;
      }

      now = Clock::now();

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
        translate_input(event, input->device_index);
      }

      if (g_input_timeout_start &&
          now >= g_input_timeout_start.value() + g_input_timeout) {
        g_input_timeout_start.reset();
        translate_input(make_input_timeout_event(g_input_timeout),
          g_last_device_index);
      }

      if (!g_flush_scheduled_at || now > g_flush_scheduled_at) {
        g_flush_scheduled_at.reset();
        if (!flush_send_buffer()) {
          error("Sending input failed");
          return true;
        }
      }

      // let client update configuration and context
      if (interrupt_fd >= 0)
        if (!read_client_messages(Duration::zero()) ||
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
        if (!g_uinput_device.create(uinput_device_name)) {
          error("Creating uinput device failed");
          return 1;
        }

        if (!g_grabbed_devices.grab(uinput_device_name,
              g_stage->has_mouse_mappings())) {
          error("Initializing input device grabbing failed");
          g_uinput_device = { };
          return 1;
        }

        evaluate_device_filters();

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
  if (settings.debounce)
    g_button_debouncer.emplace();

  if (!g_client.initialize()) {
    error("Initializing keymapper connection failed");
    return 1;
  }

  return connection_loop();
}
