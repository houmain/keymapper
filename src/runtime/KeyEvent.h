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
  UpAsync,     // internally (in input expression)
  DownAsync,   // internally (in input expression)
  DownMatched, // internally (in sequence)
};

using KeyCode = uint16_t;
enum {
  no_key = 0,
  any_key = 0xFF00,
  first_virtual_key = 0xFF10,
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

