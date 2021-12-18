#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

// Async means that the key can be pressed/released any time afterwards (but
// does not have to). A normal up/down can follow, to "synchronize" the state.
// So *A *B +A +B means that A can be pressed, then B can also be pressed,
// but finally both A and B have to be pressed.
// "Not" in input means, that the key must not be pressed to match.
// In output it ensures the key is released while the command is applied.
enum class KeyState : uint16_t {
  Up,
  Down,
  Not,
  UpAsync,         // only in input expression
  DownAsync,       // only in input expression
  OutputOnRelease, // only in output expression
  DownMatched,     // only in sequence
};

using KeyCode = uint16_t;
enum {
  no_key            = 0,
  any_key           = 0xF000,
  first_virtual_key = 0xF100,
  first_action_key  = 0xF200,
  first_logical_key = 0xF300,
};

struct KeyEvent {
  KeyCode key{ };
  KeyState state{ KeyState::Down };

  KeyEvent() = default;
  KeyEvent(KeyCode key, KeyState state)
    : key(key), state(state) {
  }
  bool operator==(const KeyEvent& b) const {
    return (key == b.key && state == b.state);
  }
  bool operator!=(const KeyEvent& b) {
    return !(*this == b);
  }
};

class KeySequence : public std::vector<KeyEvent> {
public:
  KeySequence() = default;
  KeySequence(std::initializer_list<KeyEvent> keys)
    : std::vector<KeyEvent>(keys) {
  }
};

template<typename It>
class Range {
public:
  using Iterator = It;

  template<typename T>
  Range(T& range)
    : m_begin(range.begin()),
      m_end(range.end()) {
  }
  Range(const Iterator& begin, const Iterator& end)
    : m_begin(begin),
      m_end(end) {
  }

  const Iterator& begin() const { return m_begin; }
  const Iterator& end() const { return m_end; }
  bool empty() const { return m_begin == m_end; }
  size_t size() const { return m_end - m_begin; }
  auto operator[](size_t index) const { return *(m_begin + index); }
  void pop_back() { --m_end; }

private:
  Iterator m_begin;
  Iterator m_end;
};

using ConstKeySequenceRange = Range<KeySequence::const_iterator>;
using KeySequenceRange = Range<KeySequence::iterator>;

inline bool is_virtual_key(KeyCode key) {
  return (key >= first_virtual_key && key < first_action_key);
}

inline bool is_action_key(KeyCode key) {
  return (key >= first_action_key);
}
