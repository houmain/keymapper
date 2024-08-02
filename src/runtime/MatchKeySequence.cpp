
#include "MatchKeySequence.h"
#include <cassert>
#include <algorithm>

namespace {
  bool unifiable(KeyState a, KeyState b) {
    if (a == KeyState::DownMatched)
      a = KeyState::Down;
    if (b == KeyState::DownMatched)
      b = KeyState::Down;
    return (a == b);
  }

  bool unifiable(Key a, Key b) {
    if (a == Key::none || b == Key::none)
      return false;
    if (a == b)
      return true;
    // do not let Any match timeout
    if (a == Key::timeout || b == Key::timeout)
      return false;
    if (a == Key::any && is_keyboard_key(b))
      return true;
    if (b == Key::any && is_keyboard_key(a))
      return true;
    return false;
  }

  // not commutative, first parameter needs to be input sequence
  bool timeout_unifiable(const KeyEvent& se, const KeyEvent& ee) {
    const auto time_reached = (se.value >= ee.value);
    const auto is_not = (is_not_timeout(se.state) || is_not_timeout(ee.state));
    return (is_not ? !time_reached : time_reached);
  }

  bool unifiable(const KeyEvent& a, const KeyEvent& b) {
    // do not let Any match again
    if (a.key == Key::any && b.state == KeyState::DownMatched)
      return false;
    if (b.key == Key::any && a.state == KeyState::DownMatched)
      return false;
    if (!unifiable(a.key, b.key))
      return false;
    if (a.key == Key::timeout)
      return timeout_unifiable(a, b);
    return unifiable(a.state, b.state);
  }
} // namespace

MatchResult MatchKeySequence::operator()(ConstKeySequenceRange expression,
                                         ConstKeySequenceRange sequence,
                                         std::vector<Key>* any_key_matches,
                                         KeyEvent* input_timeout_event) const {
  assert(!expression.empty() && !sequence.empty());
  assert(any_key_matches && input_timeout_event);
  any_key_matches->clear();

  const auto matches_none = KeyEvent(Key::none, KeyState::Up);
  auto e = 0u;
  auto s = 0u;
  auto is_no_might_match = false;
  m_async.clear();
  m_not_keys.clear();
  m_ignore_ups.clear();

  while (e < expression.size() || s < sequence.size()) {
    const auto& se = (s < sequence.size() ? sequence[s] : matches_none);
    const auto& ee = (e < expression.size() ? expression[e] : matches_none);
    const auto async_state =
      (se.state == KeyState::Up ? KeyState::UpAsync : KeyState::DownAsync);

    // undo adding to Not keys
    if (ee.state == KeyState::Down)
      m_not_keys.erase(
        std::remove(m_not_keys.begin(), m_not_keys.end(), ee.key), m_not_keys.end());

    // check if key must not be down
    if ((se.state == KeyState::Down || 
         se.state == KeyState::DownMatched) &&
        std::count(m_not_keys.begin(), m_not_keys.end(), se.key))
      return MatchResult::no_match;;

    if (ee.state == KeyState::DownAsync ||
        ee.state == KeyState::UpAsync) {
      m_async.push_back(ee);
      ++e;
    }
    else if (ee.state == KeyState::Not && ee.key != Key::timeout) {
      // add to Not keys
      m_not_keys.push_back(ee.key);
      ++e;
    }
    else if (unifiable(se, ee)) {
      // direct match
      ++s;
      ++e;

      if (ee.key == Key::any && se.state == KeyState::Down)
        any_key_matches->push_back(se.key);

      // remove from async
      m_async.erase(std::remove_if(begin(m_async), end(m_async),
        [&](const KeyEvent& e) { return (se.key == e.key); }), 
        end(m_async));
    }
    else if (ee.key == Key::timeout && se == matches_none) {
      // when a timeout is encountered and sequence ended
      *input_timeout_event = ee;
      return MatchResult::might_match;
    }
    else if (ee.state == KeyState::NoMightMatch) {
      is_no_might_match = true;
      ++e;
    }
    else {
      // when matching history, do not match again with optional events
      if (is_no_might_match && ee == matches_none)
        return MatchResult::no_match;

      // try to match sequence event with async
      auto it = std::find_if(begin(m_async), end(m_async),
        [&](const KeyEvent& e) {
          return (e.state == async_state &&
            unifiable(se.key, e.key));
        });

      if (it != end(m_async)) {
        // mark async as matched
        it->state = se.state;
        ++s;
        continue;
      }

      if (se.state == KeyState::DownMatched) {
        // ignore already matched events in sequence
        ++s;
        continue;
      }

      // try to match expression event with async
      it = std::find_if(begin(m_async), end(m_async),
        [&](const KeyEvent& e) { return unifiable(ee, e); });

      if (it != end(m_async)) {
        // remove async
        m_async.erase(it);
        ++e;
        continue;
      }

      if (ee.state == KeyState::Down) {
        // look for unmatched async up and async down
        // which means that it does not matter if key was released in between
        it = std::find(begin(m_async), end(m_async), 
          KeyEvent(ee.key, KeyState::DownAsync));
        if (it != end(m_async) && 
            it != begin(m_async) && 
            *std::prev(it) == KeyEvent(ee.key, KeyState::UpAsync)) {
          ++e;
          continue;
        }
      }

      if (is_no_might_match) {
        if (e == 1) {
          // ignore additional events at the front of history
          if (se != matches_none) {
            if (se.state == KeyState::Down)
              m_ignore_ups.push_back(se.key);
            ++s;
            continue;
          }
          // still only matched NoMightMatch
          return MatchResult::no_match;
        }
        else {
          // also ignore Ups of ignored Downs
          if (se.state == KeyState::Up &&
              std::count(m_ignore_ups.begin(), m_ignore_ups.end(), se.key)) {
            ++s;
            continue;
          }
        }
      }

      // no match with async
      const auto might_match = (s >= sequence.size());
      return (might_match ? MatchResult::might_match :
          MatchResult::no_match);
    }
  }
  return MatchResult::match;
}

