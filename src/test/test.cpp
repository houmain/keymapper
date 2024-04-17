
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "test.h"
#include "config/ParseKeySequence.h"
#include "config/ParseConfig.h"
#include "runtime/Key.h"
#include "runtime/Timeout.h"

namespace {
  struct Stream : std::stringstream {
    bool first = true;

    template<typename T>
    Stream& operator<<(const T& v) {
      if (!std::exchange(first, false))
        static_cast<std::ostream&>(*this) << ' ';
      static_cast<std::ostream&>(*this) << v;
      return *this;
    }
  };
} // namespace

void message(const char* format, ...) { }
void error(const char* format, ...) { }
void verbose(const char* format, ...) {
#if 0
  va_list args;
  va_start(args, format);
  vfprintf(stdout, format, args);
  va_end(args);
  fputc('\n', stdout);
  fflush(stdout);
#endif
}

std::ostream& operator<<(std::ostream& os, const KeyEvent& event) {
  switch (event.state) {
    case KeyState::Up: os << '-'; break;
    case KeyState::Down: os << '+'; break;
    case KeyState::UpAsync: os << '~'; break;
    case KeyState::DownAsync: os << '*'; break;
    case KeyState::Not: os << '!'; break;
    case KeyState::DownMatched: os << '#'; break;
    case KeyState::OutputOnRelease: os << '^'; break;
    case KeyState::NotTimeout_cancel_on_up_down: os << '?'; break;
  }

  if (is_virtual_key(event.key)) {
    os << "Virtual" << (*event.key - *Key::first_virtual);
  }
  else if (is_action_key(event.key)) {
    os << "Action" << (*event.key - *Key::first_action);
  }
  else if (event.key == Key::timeout) {
    os << timeout_to_milliseconds(event.timeout).count() << "ms";
  }
  else if (auto name = get_key_name(event.key)) {
    os << name;
  }
  else {
    os << "???";
  }
  return os;
}

KeySequence parse_input(const char* input) {
  static auto parse = ParseKeySequence();
  return parse(input, true);
}

KeySequence parse_output(const char* output) {
  static auto parse = ParseKeySequence();
  return parse(output, false);
}

KeySequence parse_sequence(const char* it, const char* const end) {
  auto sequence = KeySequence();
  while (it != end) {
    if (auto value = try_read_timeout(&it, end)) {
      sequence.emplace_back(
        make_input_timeout_event(std::chrono::milliseconds(*value)));
    }
    else {
      auto key_state = KeyState::Down;
      if (skip(&it, end, "-"))
        key_state = KeyState::Up;
      else if (!skip(&it, end, "+"))
        throw std::runtime_error("invalid key state");
      const auto begin = it;
      skip_ident(&it, end);
      const auto name = std::string_view(begin, std::distance(begin, it));
      auto key = get_key_by_name(name);
      if (key == Key::none)
          throw std::runtime_error("invalid key");
      sequence.emplace_back(key, key_state);
    }
    skip_space(&it, end);
  }
  return sequence;
}

std::string format_sequence(const KeySequence& sequence) {
  auto stream = Stream();
  for (const auto& event : sequence)
    stream << event;
  return stream.str();
}

std::string format_list(const std::vector<Key>& keys) {
  auto stream = Stream();
  for (auto key : keys)
    stream << get_key_name(key);
  return stream.str();
}

Stage create_stage(const char* string, bool activate_all_contexts) {
  static auto parse_config = ParseConfig();
  auto stream = std::stringstream(string);
  auto config = parse_config(stream);

  auto contexts = std::vector<Stage::Context>();
  for (auto& config_context : config.contexts) {
    auto& context = contexts.emplace_back();
    for (auto& input : config_context.inputs)
      context.inputs.push_back({ std::move(input.input), input.output_index });
    context.outputs = std::move(config_context.outputs);
    for (auto& output : config_context.command_outputs)
      context.command_outputs.push_back({ std::move(output.output), output.index });
    context.device_filter = std::move(config_context.device_filter);
    context.modifier_filter = std::move(config_context.modifier_filter);
    context.fallthrough = config_context.fallthrough;
  }
  auto stage = Stage(std::move(contexts));

  if (activate_all_contexts) {
    auto active_contexts = std::vector<int>();
    for (auto i = 0; i < static_cast<int>(stage.contexts().size()); ++i)
      active_contexts.push_back(i);
    stage.set_active_client_contexts(active_contexts);
  }
  return stage;
}

KeyEvent reply_timeout_ms(int timeout_ms) {
  return KeyEvent(Key::timeout, KeyState::Up,
    duration_to_timeout(std::chrono::milliseconds(timeout_ms)));
}

KeyEvent make_timeout_ms(int timeout_ms, bool cancel_on_up) {
  return KeyEvent(Key::timeout, (cancel_on_up ? 
    KeyState::Timeout_cancel_on_up_down : KeyState::Timeout_cancel_on_down), 
    duration_to_timeout(std::chrono::milliseconds(timeout_ms)));
}

KeyEvent make_not_timeout_ms(int timeout_ms, bool cancel_on_up) {
  return KeyEvent(Key::timeout, (cancel_on_up ? 
    KeyState::NotTimeout_cancel_on_up_down : KeyState::NotTimeout_cancel_on_down), 
    duration_to_timeout(std::chrono::milliseconds(timeout_ms)));
}

KeyEvent make_output_timeout_ms(int timeout_ms) {
  return KeyEvent(Key::timeout, KeyState::Down,
    duration_to_timeout(std::chrono::milliseconds(timeout_ms)));
}
