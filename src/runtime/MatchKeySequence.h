#pragma once

#include "KeyEvent.h"

enum class MatchResult { no_match, might_match, match };

class MatchKeySequence {
public:
  MatchResult operator()(const KeySequence& expression,
    const KeySequence& sequence) const;

private:
  // temporary buffer
  mutable std::vector<KeyEvent> m_async;
};
