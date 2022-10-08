#pragma once

#include "get_key_name.h"
#include "runtime/KeyEvent.h"
#include <stdexcept>
#include <string>
#include <functional>

// Here are some examples for input and output expressions. Each example
// consists of a description of the desired result, followed by the
// corresponding input expression (in configuration format and the low
// level format it is translated to).
//
// Input expressions:
//   A has to be pressed:
//     A          -> +A ~A
//   A has to be pressed first then B. A can still be hold:
//     A B        -> +A ~A +B ~B
//   A and B have to be pressed together, order does not matter:
//     (A B)      -> *A *B +A +B
//   A has to be pressed first then B and C together. A can be released any time:
//     A(B C)     -> +A ~A *B *C +B +C
//   A has to be pressed first then B then C. A has to be released last:
//     A{B C}     -> +A +B ~B +C
//   A has to be pressed first then B and C together. A has to be released last:
//     A{(B C)}   -> +A *B *C +B +C
//   A and B have to be pressed together, order does not matter. Both keys have
//   to be hold while C and D are pressed:
//     (A B){C D} -> *A *B +A +B +C ~C +D
//
// Output expressions:
//   Press A:
//     A          -> +A
//   Hold A while pressing B:
//     A{B}       -> +A +B
//   Press C while holding A and B:
//     (A B){C}   -> +A +B +C

struct ParseError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

class ParseKeySequence {
public:
  using GetKeyByName = std::function<Key(std::string_view)>;
  using AddTerminalCommand = std::function<Key(std::string_view)>;

  KeySequence operator()(const std::string& str, bool is_input,
    GetKeyByName get_key_by_name = ::get_key_by_name,
    AddTerminalCommand add_terminal_command = { });

private:
  using It = std::string::const_iterator;

  void parse(It it, const It end);
  Key read_key(It* it, const It end);
  void add_key_to_sequence(Key key, KeyState state);
  void add_key_to_buffer(Key key);
  bool remove_from_keys_not_up(Key key);
  void flush_key_buffer(bool up_immediately, bool up_sync = false);
  void up_any_keys_not_up_yet();
  bool all_pressed_at_once() const;
  void remove_any_up_from_end();

  bool m_is_input{ };
  GetKeyByName m_get_key_by_name;
  AddTerminalCommand m_add_terminal_command;
  std::vector<Key> m_keys_not_up;
  std::vector<Key> m_key_buffer;
  KeySequence m_sequence;
};
