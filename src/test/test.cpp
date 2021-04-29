
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "test.h"
#include "config/ParseKeySequence.h"
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
    os << get_key_name(static_cast<Key>(event.key));
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
    auto key = get_key_by_name(std::string(begin, it));
    if (key == Key::None)
      throw std::runtime_error("invalid key");
    sequence.emplace_back(*key, key_state);
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
