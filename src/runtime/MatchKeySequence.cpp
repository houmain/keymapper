
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
    return (a == Key::any || b == Key::any);
  }

  // not commutative, first parameter needs to be input sequence
  bool timeout_unifiable(const KeyEvent& se, const KeyEvent& ee) {
    const auto time_reached = (se.timeout >= ee.timeout);
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

MatchResult MatchKeySequence::operator()(const KeySequence& expression,
                                         ConstKeySequenceRange sequence,
                                         std::vector<Key>* any_key_matches,
                                         KeyEvent* input_timeout_event) const {
  assert(!expression.empty() && !sequence.empty());
  assert(any_key_matches && input_timeout_event);
  any_key_matches->clear();

  const auto matches_none = KeyEvent(Key::none, KeyState::Down);
  auto e = 0u;
  auto s = 0u;
  m_async.clear();

  while (e < expression.size() || s < sequence.size()) {
    const auto& se = (s < sequence.size() ? sequence[s] : matches_none);
    const auto& ee = (e < expression.size() ? expression[e] : matches_none);
    const auto async_state =
      (se.state == KeyState::Up ? KeyState::UpAsync : KeyState::DownAsync);

    if (ee.state == KeyState::DownAsync ||
        ee.state == KeyState::UpAsync) {
      m_async.push_back(ee);
      ++e;
    }
    else if (ee.state == KeyState::Not && ee.key != Key::timeout) {
      // check if remaining sequence contains the not allowed key
      const auto it = std::find_if(sequence.begin() + s, sequence.end(),
        [&](const KeyEvent& e) {
          return (unifiable(e.state, KeyState::Down) &&
                  unifiable(e.key, ee.key));
        });
      if (it != sequence.end())
        return MatchResult::no_match;
      ++e;
    }
    else if (unifiable(se, ee)) {
      // direct match
      ++s;
      ++e;

      if (ee.key == Key::any && se.state == KeyState::Down)
        any_key_matches->push_back(se.key);

      // remove async (+A in sequence/expression, *A or +A in async)
      const auto it = std::find_if(cbegin(m_async), cend(m_async),
        [&](const KeyEvent& e) {
          return ((e.state == async_state || e.state == ee.state) &&
            se.key == e.key);
        });
      if (it != cend(m_async))
        m_async.erase(it);
    }
    else if (ee.key == Key::timeout && se == matches_none) {
      // when a timeout is encountered and sequence ended
      *input_timeout_event = ee;
      return MatchResult::might_match;
    }
    else {
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
      // no match with async
      const auto might_match = (s >= sequence.size());
      return (might_match ? MatchResult::might_match :
          MatchResult::no_match);
    }
  }
  return MatchResult::match;
}

