
#include "ParseKeySequence.h"
#include "../runtime/Timeout.h"
#include <algorithm>

namespace {
  Key get_logical_key(Key key) {
    switch (key) {
      case Key::ShiftLeft: 
      case Key::ShiftRight: return Key::Shift;
      case Key::ControlLeft: 
      case Key::ControlRight: return Key::Control;
      case Key::MetaLeft: 
      case Key::MetaRight: return Key::Meta;
      default: return key;
    }
  }
  
  bool has_key_down(const KeySequence& sequence) {
    return std::count_if(sequence.rbegin(), sequence.rend(), 
      [&](const KeyEvent& event) { 
        return (event.state == KeyState::Down && event.key != Key::timeout);
      });
  }
  
  bool ends_with_async_up(const KeySequence& sequence, Key key) {
    return (!sequence.empty() && sequence.back() == KeyEvent{ key, KeyState::UpAsync });
  }

  bool is_pressed(const KeySequence& sequence, Key key) {
    const auto it = std::find_if(sequence.rbegin(), sequence.rend(), 
      [&](const KeyEvent& event) { return (event.key == key); });
    return (it != sequence.rend() && it->state != KeyState::Up);
  }
} // namespace

KeySequence ParseKeySequence::operator()(
    std::string_view str, bool is_input,
    GetKeyByName get_key_by_name,
    AddTerminalCommand add_terminal_command) try {

  m_string = str;
  m_is_input = is_input;
  m_get_key_by_name = std::move(get_key_by_name);
  m_add_terminal_command = std::move(add_terminal_command);
  m_keys_not_up.clear();
  m_key_buffer.clear();
  m_sequence.clear();

  parse(cbegin(str), cend(str));

  return std::move(m_sequence);
}
catch (const std::exception& ex) {
  // append "at" showing preprocessed configuration line
  // when it contains more than a single identifier
  auto begin = m_string.begin();
  auto end = m_string.end();
  skip_space(&begin, end);
  trim_comment(begin, &end);
  auto it = begin;
  skip_ident(&it, end);
  skip_space(&it, end);
  trim_space(it, &end);
  if (it != end)
    throw ParseError(std::string(ex.what()) + 
      R"( at ")" + std::string(begin, end) + R"(")");
  throw ParseError(ex.what());
}

void ParseKeySequence::add_key_to_sequence(Key key, KeyState state) {
  m_sequence.emplace_back(key, state);
}

void ParseKeySequence::add_key_to_buffer(Key key) {
  m_key_buffer.push_back(key);
}

bool ParseKeySequence::remove_from_keys_not_up(Key key) {
  const auto size_before = m_keys_not_up.size();
  m_keys_not_up.erase(
    std::remove_if(begin(m_keys_not_up), end(m_keys_not_up),
      [&](Key k) { return k == key; }), end(m_keys_not_up));
  return (m_keys_not_up.size() != size_before);
}

void ParseKeySequence::flush_key_buffer(bool up_immediately) {
  for (const auto buffered_key : m_key_buffer) {
    m_sequence.emplace_back(buffered_key, KeyState::Down);
    if (up_immediately) {
      if (m_is_input) {
        m_sequence.emplace_back(buffered_key, KeyState::UpAsync);
      }
      else {
        m_sequence.emplace_back(buffered_key, KeyState::Up);
        remove_from_keys_not_up(buffered_key);
      }
    }
    else {
      m_keys_not_up.push_back(buffered_key);
    }
  }
  m_key_buffer.clear();
}

void ParseKeySequence::up_any_keys_not_up_yet() {
  std::for_each(m_keys_not_up.rbegin(), m_keys_not_up.rend(), [&](Key key) {
    if (m_is_input) {
      m_sequence.emplace_back(key, KeyState::UpAsync);
    }
    else {
      m_sequence.emplace_back(key, KeyState::Up);
    }
  });
  m_keys_not_up.clear();
}

void ParseKeySequence::sync_after_not_timeouts() {
  for (auto i = size_t{ }; i < m_sequence.size(); ++i)
    if (is_not_timeout(m_sequence[i])) {
      // get number of async Ups
      auto n = 0;
      for (auto j = i + 1; j < m_sequence.size() && 
          m_sequence[j].state == KeyState::UpAsync; ++j)
        ++n;
      // insert a sync Up for each
      for (auto j = 0; j < n; ++j)
        m_sequence.insert(std::next(m_sequence.begin(), i + 1 + n),
          KeyEvent(m_sequence[i + j + 1].key, KeyState::Up));
    }
}

bool ParseKeySequence::all_pressed_at_once() const {
  auto after_downs = std::find_if_not(m_sequence.begin(), m_sequence.end(),
    [](const KeyEvent& event) { return (event.state != KeyState::Up); });
  auto after_ups = std::find_if_not(after_downs, m_sequence.end(),
    [](const KeyEvent& event) { return (event.state != KeyState::Down); });
  return (after_ups == m_sequence.end());
}

void ParseKeySequence::remove_any_up_from_end() {
  while (!m_sequence.empty() &&
         m_sequence.back().state == KeyState::Up)
    m_sequence.pop_back();
}

Key ParseKeySequence::read_key(It* it, const It end) {
  auto key_name = read_ident(it, end);
  if (key_name.empty()) {
    const char at = *(*it == end ? std::prev(*it) : *it);
    throw ParseError("Key name expected at '" + std::string(1, at) + "'");
  }
  if (const auto key = m_get_key_by_name(key_name); key != Key::none)
    return key;
  throw ParseError("Invalid key '" + key_name + "'");
}

void ParseKeySequence::add_timeout_event(uint16_t timeout, bool is_not, bool cancel_on_up) {
  flush_key_buffer(true);
  if (m_is_input && !has_key_down(m_sequence))
    throw ParseError("Input sequence must not start with timeout");
  if (!m_is_input && is_not)
    throw ParseError("Ouput sequence must not contain a not-timeout");

  // in output expressions always use state Down
  // in input expressions use Up or Down depending on what should cancel a timeout:
  const auto state = (!m_is_input ? KeyState::Down : 
    (is_not ? (cancel_on_up ? KeyState::NotTimeout_cancel_on_up_down : 
                              KeyState::NotTimeout_cancel_on_down) :
              (cancel_on_up ? KeyState::Timeout_cancel_on_up_down : 
                              KeyState::Timeout_cancel_on_down)));

  // try to merge with previous timeout
  if (!m_sequence.empty() &&
      m_sequence.back().key == Key::timeout &&
      m_sequence.back().state == state)
    m_sequence.back().timeout = sum_timeouts(m_sequence.back().timeout, timeout);
  else
    m_sequence.emplace_back(Key::timeout, state, timeout);
}

bool ParseKeySequence::add_string_typing(std::string_view string) {
  if (!m_string_typer.has_value())
    m_string_typer.emplace();

  flush_key_buffer(true);
  add_key_to_sequence(Key::Meta, KeyState::Not);

  auto prev_modifiers = StringTyper::Modifiers{ };
  auto initial = true;
  auto has_modifier = false;
  auto added_not_for_modifiers = StringTyper::Modifiers{ };
  m_string_typer->type(string,
    [&](Key key, StringTyper::Modifiers modifiers) {
      const auto add_or_release = [&](auto mod, auto key) {
        // initially any modifier might be pressed, add a Not for unneeded
        const auto changed = 
          (initial ? ~StringTyper::Modifiers{} : 
          (modifiers ^ prev_modifiers));
        if (changed & mod) {
          if (modifiers & mod) {
            add_key_to_sequence(key, KeyState::Down);
          }
          else {
            if (!initial)
              add_key_to_sequence(key, KeyState::Up);

            // add Not once initially or after first Up
            if (!(added_not_for_modifiers & mod)) {
              add_key_to_sequence(get_logical_key(key), KeyState::Not); 
              added_not_for_modifiers |= mod;
            }
          }
        }
      };
      add_or_release(StringTyper::Shift, Key::ShiftLeft);
      add_or_release(StringTyper::Alt, Key::AltLeft);
      add_or_release(StringTyper::AltGr, Key::AltRight);
      add_or_release(StringTyper::Control, Key::ControlLeft);
      add_key_to_sequence(key, KeyState::Down);
      add_key_to_sequence(key, KeyState::Up);
      has_modifier |= (modifiers != 0);
      prev_modifiers = modifiers;
      initial = false;
    });

  if (prev_modifiers & StringTyper::Shift)
    add_key_to_sequence(Key::ShiftLeft, KeyState::Up);
  if (prev_modifiers & StringTyper::Alt)
    add_key_to_sequence(Key::AltLeft, KeyState::Up);
  if (prev_modifiers & StringTyper::AltGr)
    add_key_to_sequence(Key::AltRight, KeyState::Up);
  if (prev_modifiers & StringTyper::Control)
    add_key_to_sequence(Key::ControlLeft, KeyState::Up);

  return has_modifier;
}

void ParseKeySequence::check_ContextActive_usage() {
  const auto it = std::find_if(m_sequence.begin(), m_sequence.end(),
    [](const KeyEvent& e) { return e.key == Key::ContextActive; });
  if (it != m_sequence.end()) {
    if (!m_is_input)
      throw ParseError("ContextActive is only allowed in input");
    if (m_sequence.size() != 2)
      throw ParseError("ContextActive can only be used alone");
    m_sequence.pop_back();
  }
}

void ParseKeySequence::parse(It it, const It end) {
  auto output_on_release = false;
  auto in_together_group = false;
  auto in_modified_group = 0;
  auto has_modifier = false;
  for (;;) {
    skip_space(&it, end);
    if (skip(&it, end, "!")) {

      if (auto timeout = try_read_timeout(&it, end)) {
        if (in_together_group)
          throw ParseError("Timeout not allowed in group");
        add_timeout_event(*timeout, true, in_modified_group);
        continue;
      }

      if (in_together_group)
        throw ParseError("Unexpected '!'");

      const auto key = read_key(&it, end);

      // prevent A{!A} but allow A{B !B}
      if (m_is_input && in_modified_group)
        if (!std::count(m_key_buffer.begin(), m_key_buffer.end(), key))
          throw ParseError("Key to up not in modifier group");

      flush_key_buffer(m_is_input);

      if (remove_from_keys_not_up(key))
        add_key_to_sequence(key, KeyState::UpAsync);

      // in input sequences conditionally insert Up or Not 
      // depending on whether there was a Down
      if (m_is_input && ends_with_async_up(m_sequence, key))
        m_sequence.back().state = KeyState::Up;
      else if (m_is_input && is_pressed(m_sequence, key))
        add_key_to_sequence(key, KeyState::Up);
      else
        add_key_to_sequence(key, KeyState::Not);
    }
    else if (skip(&it, end, "'") || skip(&it, end, "\"")) {
      char quote[2] = { *std::prev(it), '\0' };
      if (m_is_input || in_together_group || in_modified_group)
        throw ParseError("Unexpected string");
      const auto begin = it;
      if (!skip_until(&it, end, quote))
        throw ParseError("Unterminated string");
      has_modifier |= add_string_typing(
        std::string_view(&*begin, std::distance(begin, it) - 1));
    }
    else if (skip(&it, end, "$")) {
      if (m_is_input || in_together_group || in_modified_group)
        throw ParseError("Unexpected '$'");
      if (!skip(&it, end, "("))
        throw ParseError("Expected '('");
      const auto begin = it;
      for (auto level = 1; level > 0; ++it) {
        if (it == end)
          throw ParseError("Expected ')'");
        if (*it == '(')
          ++level;
        else if (*it == ')')
          --level;
      }
      flush_key_buffer(true);
      add_key_to_sequence(m_add_terminal_command(
          std::string_view(&*begin, std::distance(begin, it) - 1)),
        KeyState::Down);
    }
    else if (skip(&it, end, "^")) {
      if (m_is_input || output_on_release ||
          in_together_group || in_modified_group)
        throw ParseError("Unexpected '^'");

      flush_key_buffer(true);
      add_key_to_sequence(Key::none, KeyState::OutputOnRelease);
      output_on_release = true;
    }
    else if (skip(&it, end, "(")) {
      // begin together-group
      if (in_together_group)
        throw ParseError("Unexpected '('");

      flush_key_buffer(true);
      in_together_group = true;
    }
    else if (skip(&it, end, ")")) {
      // end together-group
      if (!in_together_group)
        throw ParseError("Unexpected ')'");

      if (m_is_input) {
        // *A *B +A +B
        for (const auto key : m_key_buffer)
          m_sequence.emplace_back(key, KeyState::DownAsync);
      }
      flush_key_buffer(false);
      in_together_group = false;
    }
    else if (skip(&it, end, "{")) {
      // begin modified-group
      flush_key_buffer(false);
      if (m_keys_not_up.empty())
        throw ParseError("Unexpected '{'");
      ++in_modified_group;
      has_modifier = true;
    }
    else if (skip(&it, end, "}")) {
      // end modified-group
      if (!in_modified_group)
        throw ParseError("Unexpected '}'");

      flush_key_buffer(true);
      up_any_keys_not_up_yet();
      --in_modified_group;
    }
    else if (skip(&it, end, "#") || skip(&it, end, ";")) {
      flush_key_buffer(true);
      break;
    }
    else if (auto timeout = try_read_timeout(&it, end)) {
      if (in_together_group)
        throw ParseError("Timeout not allowed in group");
      add_timeout_event(*timeout, false, in_modified_group);
    }
    else {
      if (!in_together_group ||
          it == end)
        flush_key_buffer(true);

      // done?
      if (it == end)
        break;
      add_key_to_buffer(read_key(&it, end));
    }
  }

  if (in_together_group)
    throw ParseError("Expected ')'");
  if (in_modified_group)
    throw ParseError("Expected '}'");

  if (m_is_input) {
    sync_after_not_timeouts();
  }
  else {
    up_any_keys_not_up_yet();
    if (!has_modifier && all_pressed_at_once())
      remove_any_up_from_end();
  }

  if (m_is_input && !has_key_down(m_sequence))
    throw ParseError("Sequence contains no key down");

  check_ContextActive_usage();
}
