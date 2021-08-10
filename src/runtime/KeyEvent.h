#pragma once

#include <cstdint>
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

inline bool is_virtual_key(KeyCode key) {
  return (key >= first_virtual_key && key < first_action_key);
}

inline bool is_action_key(KeyCode key) {
  return (key >= first_action_key);
}
