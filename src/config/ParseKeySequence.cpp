
#include "ParseKeySequence.h"
#include "string_iteration.h"
#include "../runtime/Timeout.h"
#include <algorithm>
#include <optional>

namespace {
  template<typename ForwardIt>
  std::optional<uint16_t> try_read_timeout(ForwardIt* it, ForwardIt end) {
    const auto begin = *it;
    const auto number = read_number(it, end);
    if (*it != begin && skip(it, end, "ms"))
      return duration_to_timeout(std::chrono::milliseconds(number));
    *it = begin;
    return std::nullopt;
  }
} // namespace

KeySequence ParseKeySequence::operator()(
    const std::string& str, bool is_input,
    GetKeyByName get_key_by_name,
    AddTerminalCommand add_terminal_command) {

  m_is_input = is_input;
  m_get_key_by_name = std::move(get_key_by_name);
  m_add_terminal_command = std::move(add_terminal_command);
  m_keys_not_up.clear();
  m_key_buffer.clear();
  m_sequence.clear();

  parse(cbegin(str), cend(str));

  return std::move(m_sequence);
}

void ParseKeySequence::add_key_to_sequence(Key key, KeyState state) {
  m_sequence.emplace_back(key, state);
  if (state == KeyState::Up)
    m_keys_not_up.push_back(key);
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

void ParseKeySequence::sync_adjacent_to_timeout() {
  // sync before every timeout
  for (auto i = 1u; i < m_sequence.size(); ++i)
    if (m_sequence[i].key == Key::timeout)
      if (m_sequence[i - 1].state == KeyState::UpAsync)
        m_sequence[i - 1].state = KeyState::Up;

  // sync after not-timeouts
  for (auto i = 1u; i + 1 < m_sequence.size(); ++i)
    if (m_sequence[i].key == Key::timeout &&
        m_sequence[i].state == KeyState::Not)
      if (m_sequence[i + 1].state == KeyState::UpAsync)
        m_sequence[i + 1].state = KeyState::Up;
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

void ParseKeySequence::add_timeout_event(KeyState state, uint16_t timeout) {
  if (!m_is_input)
    throw ParseError("Timeouts are only supported in input");
  flush_key_buffer(true);
  if (m_sequence.empty())
    throw ParseError("Input sequence must not start with timeout");

  // try to merge with previous timeout
  if (m_sequence.back().key == Key::timeout &&
      m_sequence.back().state == state)
    m_sequence.back().timeout = sum_timeouts(m_sequence.back().timeout, timeout);
  else
    m_sequence.emplace_back(Key::timeout, state, timeout);
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
        add_timeout_event(KeyState::Not, *timeout);
        continue;
      }

      if (in_together_group || in_modified_group)
        throw ParseError("Unexpected '!'");

      flush_key_buffer(false);

      const auto key = read_key(&it, end);
      if (remove_from_keys_not_up(key))
        add_key_to_sequence(key, KeyState::UpAsync);

      add_key_to_sequence(key, KeyState::Not);
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
      add_timeout_event(KeyState::Up, *timeout);
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
    sync_adjacent_to_timeout();
  }
  else {
    up_any_keys_not_up_yet();
    if (!has_modifier && all_pressed_at_once())
      remove_any_up_from_end();
  }
}
