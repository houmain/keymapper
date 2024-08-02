#pragma once

#include "KeyEvent.h"

enum class MatchResult { no_match, might_match, match };

class MatchKeySequence {
public:
  MatchResult operator()(
    ConstKeySequenceRange expression,
    ConstKeySequenceRange sequence,
    std::vector<Key>* any_key_matches,
    KeyEvent* input_timeout_event) const;

private:
  // temporary buffer
  mutable std::vector<KeyEvent> m_async;
  mutable std::vector<Key> m_not_keys;
  mutable std::vector<Key> m_ignore_ups;
};
