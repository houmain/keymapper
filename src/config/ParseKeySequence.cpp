
#include "ParseKeySequence.h"
#include "../runtime/Timeout.h"
#include <algorithm>

namespace {  
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

  bool has_virtual_key(const KeySequence& sequence) {
    for (const auto& event : sequence)
      if (is_virtual_key(event.key))
        return true;
    return false;
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
  m_pressed_keys.clear();
  m_keys_pressed_before_modifier_group.clear();
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
      R"( at ')" + std::string(begin, end) + R"(')");
  throw ParseError(ex.what());
}

void ParseKeySequence::add_key_to_sequence(Key key, KeyState state, KeyEvent::value_t value) {
  m_sequence.emplace_back(key, state, value);
}

void ParseKeySequence::add_key_to_buffer(Key key) {
  m_key_buffer.push_back(key);
}

bool ParseKeySequence::remove_from_pressed_keys(Key key) {
  const auto size_before = m_pressed_keys.size();
  m_pressed_keys.erase(
    std::remove_if(begin(m_pressed_keys), end(m_pressed_keys),
      [&](Key k) { return k == key; }), end(m_pressed_keys));
  return (m_pressed_keys.size() != size_before);
}

void ParseKeySequence::flush_key_buffer(bool up_immediately) {
  for (auto buffered_key : m_key_buffer)
    m_sequence.emplace_back(buffered_key, KeyState::Down);

  if (up_immediately) {
    for (auto buffered_key : m_key_buffer) {
      if (m_is_input) {
        m_sequence.emplace_back(buffered_key, KeyState::UpAsync);
      }
      else {
        m_sequence.emplace_back(buffered_key, KeyState::Up);
        remove_from_pressed_keys(buffered_key);
      }
    }
  }
  else {
    for (auto buffered_key : m_key_buffer)
      m_pressed_keys.push_back(buffered_key);
  }
  m_key_buffer.clear();
}

void ParseKeySequence::release_pressed_keys(size_t keep_keys_pressed) {
  while (m_pressed_keys.size() > keep_keys_pressed) {
    m_sequence.emplace_back(m_pressed_keys.back(),
      (m_is_input ? KeyState::UpAsync : KeyState::Up));
    m_pressed_keys.pop_back();
  }
}

void ParseKeySequence::sync_after_not_timeout() {
  // find UpAsyncs after last not-timeout and append an Up for each
  const auto last = static_cast<int>(m_sequence.size()) - 1;
  for (auto i = last; i >= 0; --i) {
    if (is_not_timeout(m_sequence[i])) {
      for (auto j = i + 1; j <= last; ++j)
        m_sequence.emplace_back(m_sequence[j].key, KeyState::Up);
      break;
    }
    else if (m_sequence[i].state != KeyState::UpAsync) {
      break;
    }
  }
}

bool ParseKeySequence::all_pressed_at_once() const {
  auto after_downs = std::find_if_not(m_sequence.begin(), m_sequence.end(),
    [](const KeyEvent& event) { return (event.state != KeyState::Up); });
  auto after_ups = std::find_if_not(after_downs, m_sequence.end(),
    [](const KeyEvent& event) { return (event.state != KeyState::Down); });
  return (after_ups == m_sequence.end());
}

void ParseKeySequence::remove_all_from_end(KeyState state) {
  while (!m_sequence.empty() &&
         m_sequence.back().state == state)
    m_sequence.pop_back();
}

Key ParseKeySequence::read_key(It* it, const It end) {
  auto key_name = read_ident(it, end);
  if (key_name.empty()) {
    const char at = *(*it == end ? std::prev(*it) : *it);
    if (at < 32 || at > 127)
      throw ParseError("Invalid character");
    throw ParseError("Unexpected symbol " + std::string(1, at));
  }
  if (const auto key = m_get_key_by_name(key_name); key != Key::none)
    return key;
  throw ParseError("Invalid key '" + key_name + "'");
}

void ParseKeySequence::add_timeout_event(KeyEvent::value_t timeout, 
    bool is_not, bool cancel_on_up) {
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
    m_sequence.back().value = sum_timeouts(m_sequence.back().value, timeout);
  else
    m_sequence.emplace_back(Key::timeout, state, timeout);
}

StringTyper& ParseKeySequence::string_typer() {
  if (!m_string_typer.has_value())
    m_string_typer.emplace();
  return m_string_typer.value();
}

bool ParseKeySequence::add_string_typing_input(
    std::string_view string, bool in_group) {
  auto prev_modifiers = StringTyper::Modifiers{ };
  auto has_modifier = false;
  auto first = true;
  string_typer().type(string,
    [&](Key key, StringTyper::Modifiers modifiers, KeyEvent::value_t value) {
      const auto press_or_release = [&](auto mod, auto key) {
        // do not change modifier state in group
        if (in_group)
          return;
        const auto changed = (modifiers ^ prev_modifiers);
        if (modifiers & mod) {
          if (prev_modifiers & mod)
            add_key_to_sequence(key, KeyState::UpAsync);
          add_key_to_sequence(key, KeyState::DownAsync);
          add_key_to_sequence(key, KeyState::Down);
        }
        else {
          if (first)
            add_key_to_sequence(key, KeyState::Not);
          if (changed & mod)  
            add_key_to_sequence(key, KeyState::Up);
        }
      };
      press_or_release(StringTyper::Shift, Key::Shift);
      press_or_release(StringTyper::Alt, Key::AltLeft);
      press_or_release(StringTyper::AltGr, Key::AltRight);
      press_or_release(StringTyper::Control, Key::Control);
      add_key_to_sequence(key, KeyState::Down);
      add_key_to_sequence(key, KeyState::UpAsync);
      has_modifier |= (modifiers != 0);
      prev_modifiers = modifiers;
      first = false;
    });

  if (prev_modifiers & StringTyper::Shift)
    add_key_to_sequence(Key::Shift, KeyState::UpAsync);
  if (prev_modifiers & StringTyper::Alt)
    add_key_to_sequence(Key::AltLeft, KeyState::UpAsync);
  if (prev_modifiers & StringTyper::AltGr)
    add_key_to_sequence(Key::AltRight, KeyState::UpAsync);
  if (prev_modifiers & StringTyper::Control)
    add_key_to_sequence(Key::Control, KeyState::UpAsync);

  return has_modifier;
}

bool ParseKeySequence::add_string_typing_output(
    std::string_view string, bool in_group) {
  if (!in_group)
    add_key_to_sequence(Key::any, KeyState::Not);

  auto prev_modifiers = StringTyper::Modifiers{ };
  auto has_modifier = false;
  string_typer().type(string,
    [&](Key key, StringTyper::Modifiers modifiers, KeyEvent::value_t value) {
      const auto press_or_release = [&](auto mod, auto key) {
        const auto changed = (modifiers ^ prev_modifiers);
        if (changed & mod)
          add_key_to_sequence(key, 
            (modifiers & mod ? KeyState::Down : KeyState::Up));
      };
      press_or_release(StringTyper::Shift, Key::Shift);
      press_or_release(StringTyper::Alt, Key::AltLeft);
      press_or_release(StringTyper::AltGr, Key::AltRight);
      press_or_release(StringTyper::Control, Key::Control);
      add_key_to_sequence(key, KeyState::Down, value);
      add_key_to_sequence(key, KeyState::Up, value);
      has_modifier |= (modifiers != 0);
      prev_modifiers = modifiers;
    });

  if (prev_modifiers & StringTyper::Shift)
    add_key_to_sequence(Key::Shift, KeyState::Up);
  if (prev_modifiers & StringTyper::Alt)
    add_key_to_sequence(Key::AltLeft, KeyState::Up);
  if (prev_modifiers & StringTyper::AltGr)
    add_key_to_sequence(Key::AltRight, KeyState::Up);
  if (prev_modifiers & StringTyper::Control)
    add_key_to_sequence(Key::Control, KeyState::Up);

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
  auto is_no_might_match = false;
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

      if (remove_from_pressed_keys(key) && m_is_input)
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
      if (in_together_group)
        throw ParseError("Unexpected string");
      
      char quote[2] = { *std::prev(it), '\0' };
      const auto begin = it;
      if (!skip_until(&it, end, quote))
        throw ParseError("Unterminated string");

      flush_key_buffer(true);

      const auto string = std::string_view(
        &*begin, std::distance(begin, it) - 1);
      has_modifier |= (m_is_input ? 
        add_string_typing_input(string, in_modified_group) : 
        add_string_typing_output(string, in_modified_group));
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
      if (m_add_terminal_command)
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
    else if (skip(&it, end, "?")) {
      if (!m_is_input || output_on_release ||
          !m_sequence.empty() || !m_key_buffer.empty() ||
          in_together_group)
        throw ParseError("Unexpected '?'");

      add_key_to_sequence(Key::none, KeyState::NoMightMatch);
      is_no_might_match = true;
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
      in_together_group = false;
    }
    else if (skip(&it, end, "{")) {
      // begin modified-group
      if (in_together_group)
        throw ParseError("Unexpected '{'");
      m_keys_pressed_before_modifier_group.push_back(m_pressed_keys.size());
      flush_key_buffer(false);
      if (m_pressed_keys.empty())
        throw ParseError("Unexpected '{'");
      ++in_modified_group;
      has_modifier = true;
    }
    else if (skip(&it, end, "}")) {
      // end modified-group
      if (!in_modified_group)
        throw ParseError("Unexpected '}'");

      flush_key_buffer(true);
      release_pressed_keys(m_keys_pressed_before_modifier_group.back());
      m_keys_pressed_before_modifier_group.pop_back();
      --in_modified_group;
      if (m_is_input)
        sync_after_not_timeout();
    }
    else if (skip(&it, end, "#") || skip(&it, end, ";")) {
      flush_key_buffer(true);
      break;
    }
    else if (auto timeout = try_read_timeout(&it, end)) {
      if (in_together_group)
        throw ParseError("Timeout not allowed in group");
      if (is_no_might_match)
        throw ParseError("Timeout not allowed in no-might-match sequence");
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
    if (is_no_might_match)
      remove_all_from_end(KeyState::UpAsync);
  }
  else {
    release_pressed_keys();
    if (!has_modifier && all_pressed_at_once())
      remove_all_from_end(KeyState::Up);
  }

  if (m_is_input && !has_key_down(m_sequence))
    throw ParseError("Sequence contains no key down");

  if (is_no_might_match && has_virtual_key(m_sequence))
    throw ParseError("Virtual keys not allowed in no-might-match sequence");
  check_ContextActive_usage();
}
