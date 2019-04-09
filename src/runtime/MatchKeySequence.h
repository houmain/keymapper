#pragma once

#include "KeyEvent.h"

enum class MatchResult { no_match, might_match, match };

class MatchKeySequence {
public:
  MatchResult operator()(const KeySequence& expression,
    const KeySequence& sequence);

private:
  // temporary buffer
  std::vector<KeyEvent> m_async;
};
