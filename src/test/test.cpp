
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "test.h"
#include "config/ParseKeySequence.h"
#include "config/ParseConfig.h"
#include "config/string_iteration.h"
#include "config/Key.h"

namespace {
  std::ostream& operator<<(std::ostream& os, const KeyEvent& event) {
    switch (event.state) {
      case KeyState::Up: os << '-'; break;
      case KeyState::Down: os << '+'; break;
      case KeyState::UpAsync: os << '~'; break;
      case KeyState::DownAsync: os << '*'; break;
      case KeyState::Not: os << '!'; break;
      case KeyState::DownMatched: os << '#'; break;
      case KeyState::OutputOnRelease: os << '^'; break;
    }
    if (!is_action_key(event.key)) {
      os << get_key_name(static_cast<Key>(event.key));
    }
    else {
      os << "Action" << (event.key - first_action_key);
    }
    return os;
  }

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
    auto key_state = KeyState::Down;
    if (skip(&it, end, "-"))
      key_state = KeyState::Up;
    else if (!skip(&it, end, "+"))
      throw std::runtime_error("invalid key state");
    const auto begin = it;
    skip_ident(&it, end);
    const auto key_code = get_key_by_name(std::string(begin, it));
    if (!key_code)
      throw std::runtime_error("invalid key");
    sequence.emplace_back(key_code, key_state);
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

std::string format_list(const std::vector<KeyCode>& keys) {
  auto stream = Stream();
  for (auto key : keys)
    stream << get_key_name(static_cast<Key>(key));
  return stream.str();
}

Stage create_stage(const char* string) {
  static auto parse_config = ParseConfig();
  auto stream = std::stringstream(string);
  auto config = parse_config(stream);

  auto contexts = std::vector<Stage::Context>();
  for (auto& config_context : config.contexts) {
    auto& context = contexts.emplace_back();
    for (const auto& input : config_context.inputs)
      context.inputs.push_back({ std::move(input.input), input.output_index });
    context.outputs = std::move(config_context.outputs);
    for (const auto& output : config_context.command_outputs)
      context.command_outputs.push_back({ std::move(output.output), output.index });
  }
  auto stage = Stage(std::move(contexts));

  // automatically activate all contexts
  auto active_contexts = std::vector<int>();
  for (auto i = 0; i < static_cast<int>(stage.contexts().size()); ++i)
    active_contexts.push_back(i);
  stage.set_active_contexts(active_contexts);

  return stage;
}
