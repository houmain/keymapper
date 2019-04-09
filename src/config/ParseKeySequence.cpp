
#include "ParseKeySequence.h"
#include "string_iteration.h"
#include "Key.h"
#include <algorithm>

KeySequence ParseKeySequence::operator()(const std::string& str, bool is_input) {
  m_is_input = is_input;
  m_keys_not_up.clear();
  m_key_buffer.clear();
  m_sequence.clear();

  parse(cbegin(str), cend(str));

  return std::move(m_sequence);
}

void ParseKeySequence::add_key_to_sequence(const std::string& key_name,
    KeyState state) {

  const auto key = get_key_by_name(key_name);
  if (key == Key::None)
    throw ParseError("invalid key '" + key_name + "'");

  flush_key_buffer(false);
  m_sequence.emplace_back(*key, state);

  if (state == KeyState::Up)
    m_keys_not_up.push_back(*key);
}

void ParseKeySequence::add_key_to_buffer(const std::string& key_name) {
  const auto key = get_key_by_name(key_name);
  if (key == Key::None)
    throw ParseError("invalid key '" + key_name + "'");
  m_key_buffer.push_back(*key);
}

void ParseKeySequence::remove_from_keys_not_up(KeyCode key) {
  m_keys_not_up.erase(
    std::remove_if(begin(m_keys_not_up), end(m_keys_not_up),
      [&](KeyCode k) { return k == key; }), end(m_keys_not_up));
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
  for (const auto key : m_keys_not_up) {
    if (m_is_input) {
      m_sequence.emplace_back(key, KeyState::UpAsync);
    }
    else {
      m_sequence.emplace_back(key, KeyState::Up);
    }
  }
  m_keys_not_up.clear();
}

void ParseKeySequence::remove_any_up_from_end() {
  while (!m_sequence.empty() &&
      (m_sequence.back().state == KeyState::Up ||
       m_sequence.back().state == KeyState::UpAsync))
    m_sequence.pop_back();
}

void ParseKeySequence::parse(It it, const It end) {
  auto in_together_group = false;
  auto in_modified_group = 0;
  for (;;) {
    skip_space(&it, end);
    if (skip(&it, end, "!")) {
      add_key_to_sequence(read_ident(&it, end), KeyState::Not);
    }
    else if (skip(&it, end, "(")) {
      // begin together-group
      if (in_together_group)
        throw ParseError("unexpected '('");

      flush_key_buffer(true);
      in_together_group = true;
    }
    else if (skip(&it, end, ")")) {
      // end together-group
      if (!in_together_group)
        throw ParseError("unexpected ')'");

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
      ++in_modified_group;
    }
    else if (skip(&it, end, "}")) {
      // end modified-group
      if (!in_modified_group)
        throw ParseError("unexpected '}'");

      flush_key_buffer(true);
      up_any_keys_not_up_yet();
      --in_modified_group;
    }
    else {
      if (!in_together_group ||
          it == end)
        flush_key_buffer(true);

      // done?
      if (it == end)
        break;
      add_key_to_buffer(read_ident(&it, end));
    }
  }

  if (in_together_group)
    throw ParseError("expected ')'");
  if (in_modified_group)
    throw ParseError("expected '}'");

  remove_any_up_from_end();
}
