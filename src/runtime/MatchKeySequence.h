#pragma once

#include "KeyEvent.h"

enum class MatchResult { no_match, might_match, match };

class MatchKeySequence {
public:
  MatchResult operator()(
    const KeySequence& expression,
    ConstKeySequenceRange sequence,
    std::vector<Key>* any_key_matches,
    KeyEvent* input_timeout_event) const;

private:
  // temporary buffer
  mutable std::vector<KeyEvent> m_async;
};
