
#include "test.h"
#include "runtime/MatchKeySequence.h"

namespace  {
  MatchResult match(const KeySequence& expression,
      const KeySequence& sequence,
      std::vector<Key>* any_key_matches,
      KeyEvent* input_timeout_event) {
    static auto match = MatchKeySequence();

    auto any_key_matches_tmp = std::vector<Key>();
    if (!any_key_matches)
      any_key_matches = &any_key_matches_tmp;

    auto input_timeout_event_tmp = KeyEvent();
    if (!input_timeout_event)
      input_timeout_event = &input_timeout_event_tmp;

    return match(expression, sequence, any_key_matches, input_timeout_event);
  }

  MatchResult match(const KeySequence& expression,
      const KeySequence& sequence) {
    return match(expression, sequence, nullptr, nullptr);
  }

  MatchResult match_get_any_keys(const KeySequence& expression,
      const KeySequence& sequence, std::vector<Key>* any_key_matches) {
    return match(expression, sequence, any_key_matches, nullptr);
  }

  MatchResult match_get_timeout_event(const KeySequence& expression,
      const KeySequence& sequence, KeyEvent* input_timeout_event) {
    return match(expression, sequence, nullptr, input_timeout_event);
  }
} // namespace

//--------------------------------------------------------------------

TEST_CASE("Match status", "[MatchKeySequence]") {
  auto expr = KeySequence();

  REQUIRE_NOTHROW(expr = parse_input("A"));
  CHECK(match(expr, parse_sequence("+A")) == MatchResult::match);
  CHECK(match(expr, parse_sequence("+B")) == MatchResult::no_match);

  // A has to be pressed first then B. A can still be hold.
  // "A B"  =>  +A ~A +B
  REQUIRE_NOTHROW(expr = parse_input("A B"));
  CHECK(match(expr, parse_sequence("+A")) == MatchResult::might_match);
  CHECK(match(expr, parse_sequence("+A -A")) == MatchResult::might_match);
  CHECK(match(expr, parse_sequence("+A +B")) == MatchResult::match);
  CHECK(match(expr, parse_sequence("+A -A +B")) == MatchResult::match);
  CHECK(match(expr, parse_sequence("+B")) == MatchResult::no_match);

  // A has to be pressed first then B. A must not be released in between.
  // "A{B}"  =>  +A +B
  REQUIRE_NOTHROW(expr = parse_input("A{B}"));
  CHECK(match(expr, parse_sequence("+A")) == MatchResult::might_match);
  CHECK(match(expr, parse_sequence("+B")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+A -A")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+A +B")) == MatchResult::match);

  // A and B have to be pressed together, order does not matter.
  // "(A B)"  =>  *A *B +A +B
  REQUIRE_NOTHROW(expr = parse_input("(A B)"));
  CHECK(match(expr, parse_sequence("+A")) == MatchResult::might_match);
  CHECK(match(expr, parse_sequence("+B")) == MatchResult::might_match);
  CHECK(match(expr, parse_sequence("+A -A")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+B -B")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+A +B")) == MatchResult::match);
  CHECK(match(expr, parse_sequence("+B +A")) == MatchResult::match);

  // A has to be pressed first then B, then C. None must be released in between.
  // "A{B{C}}"  =>  +A +B +C
  REQUIRE_NOTHROW(expr = parse_input("A{B{C}}"));
  CHECK(match(expr, parse_sequence("+A")) == MatchResult::might_match);
  CHECK(match(expr, parse_sequence("+A +B")) == MatchResult::might_match);
  CHECK(match(expr, parse_sequence("+A +B +C")) == MatchResult::match);
  CHECK(match(expr, parse_sequence("+A -A")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+B +A +C")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+A +C +B")) == MatchResult::no_match);

  // A has to be pressed first then B then C. A has to be released last.
  // "A{B C}"  =>  +A +B ~B +C
  REQUIRE_NOTHROW(expr = parse_input("A{B C}"));
  CHECK(match(expr, parse_sequence("+A")) == MatchResult::might_match);
  CHECK(match(expr, parse_sequence("+B")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+A +B")) == MatchResult::might_match);
  CHECK(match(expr, parse_sequence("+A +C")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+A +B -B")) == MatchResult::might_match);
  CHECK(match(expr, parse_sequence("+A +B -A")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+A +B +C")) == MatchResult::match);
  CHECK(match(expr, parse_sequence("+A +B -B +C")) == MatchResult::match);

  // A has to be pressed first then B and C together. A has to be released last.
  // "A{(B C)}"  =>  +A *B *C +B +C
  REQUIRE_NOTHROW(expr = parse_input("A{(B C)}"));
  CHECK(match(expr, parse_sequence("+A")) == MatchResult::might_match);
  CHECK(match(expr, parse_sequence("+B")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+A -B")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+A +B")) == MatchResult::might_match);
  CHECK(match(expr, parse_sequence("+A +B -B")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+A +B +C")) == MatchResult::match);
  CHECK(match(expr, parse_sequence("+A +B -A +C")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+A +C")) == MatchResult::might_match);
  CHECK(match(expr, parse_sequence("+A +C -C")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+A +C +B")) == MatchResult::match);
  CHECK(match(expr, parse_sequence("+A +C -A +B")) == MatchResult::no_match);

  // A and B have to be pressed together, order does not matter. Then C, then D.
  // "(A B){C D}"  =>  *A *B +A +B +C ~C +D
  REQUIRE_NOTHROW(expr = parse_input("(A B){C D}"));
  CHECK(match(expr, parse_sequence("+A")) == MatchResult::might_match);
  CHECK(match(expr, parse_sequence("+B")) == MatchResult::might_match);
  CHECK(match(expr, parse_sequence("+A -A")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+A +B")) == MatchResult::might_match);
  CHECK(match(expr, parse_sequence("+B +A")) == MatchResult::might_match);
  CHECK(match(expr, parse_sequence("+A +B -A")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+A +B -B")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+A +B +D")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+A +B +C")) == MatchResult::might_match);
  CHECK(match(expr, parse_sequence("+A +B +C +D")) == MatchResult::match);
  CHECK(match(expr, parse_sequence("+A +B +C -C")) == MatchResult::might_match);
  CHECK(match(expr, parse_sequence("+A +B +C -C +D")) == MatchResult::match);
  CHECK(match(expr, parse_sequence("+B +A +C -C +D")) == MatchResult::match);
  CHECK(match(expr, parse_sequence("+B +A +C -C -B +D")) == MatchResult::no_match);

  // string typing "AA"  =>  +S +A ~A ~S  *S +S +A ~A ~S   (S = Shift)
  // allow both Shift{A A} and Shift{A} Shift{A}
  expr = {
    KeyEvent(Key::S, KeyState::Down),
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::A, KeyState::UpAsync),
    KeyEvent(Key::S, KeyState::UpAsync),

    KeyEvent(Key::S, KeyState::DownAsync),
    KeyEvent(Key::S, KeyState::Down),
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::A, KeyState::UpAsync),
    KeyEvent(Key::S, KeyState::UpAsync),
  };
  CHECK(match(expr, parse_sequence("+S")) == MatchResult::might_match);
  CHECK(match(expr, parse_sequence("+S -S")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+A")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+S +A")) == MatchResult::might_match);
  CHECK(match(expr, parse_sequence("+S +A -A")) == MatchResult::might_match);
  CHECK(match(expr, parse_sequence("+S +A -A +A")) == MatchResult::match);
  CHECK(match(expr, parse_sequence("+S +A -A -S +S +A")) == MatchResult::match);
  CHECK(match(expr, parse_sequence("+S +A -S +S -A +A")) == MatchResult::match);
  CHECK(match(expr, parse_sequence("+S +A -S -A +S +A")) == MatchResult::match);
}

//--------------------------------------------------------------------

TEST_CASE("Match Not", "[MatchKeySequence]") {
  auto expr = KeySequence();

  REQUIRE_NOTHROW(expr = parse_input("!A B C"));
  CHECK(match(expr, parse_sequence("+A")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+A +B")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+A +B +C")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+B")) == MatchResult::might_match);
  CHECK(match(expr, parse_sequence("+B +C")) == MatchResult::match);
  CHECK(match(expr, parse_sequence("+B -B +C")) == MatchResult::match);

  REQUIRE_NOTHROW(expr = parse_input("B !A C"));
  CHECK(match(expr, parse_sequence("+A")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+A +B")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+A +B +C")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+B")) == MatchResult::might_match);
  CHECK(match(expr, parse_sequence("+B +A")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+B +C")) == MatchResult::match);

  REQUIRE_NOTHROW(expr = parse_input("B C !A"));
  CHECK(match(expr, parse_sequence("+A")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+A +B")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+A +B +C")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+B")) == MatchResult::might_match);
  CHECK(match(expr, parse_sequence("+B +C")) == MatchResult::match);

  REQUIRE_NOTHROW(expr = parse_input("A !A B"));
  CHECK(match(expr, parse_sequence("+A")) == MatchResult::might_match);
  CHECK(match(expr, parse_sequence("+A +B")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+A +C")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+A -A")) == MatchResult::might_match);
  CHECK(match(expr, parse_sequence("+A -A +A")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+A -A +B")) == MatchResult::match);
  CHECK(match(expr, parse_sequence("+A -A +C")) == MatchResult::no_match);
}

//--------------------------------------------------------------------

TEST_CASE("Match ANY", "[MatchKeySequence]") {
  auto expr = KeySequence();
  auto any_key_matches = std::vector<Key>();

  REQUIRE_NOTHROW(expr = parse_input("Any"));
  CHECK(match_get_any_keys(expr, parse_sequence("+A"),
    &any_key_matches) == MatchResult::match);
  CHECK(format_list(any_key_matches) == "A");

  REQUIRE_NOTHROW(expr = parse_input("B{Any}"));
  CHECK(match_get_any_keys(expr, parse_sequence("+A"),
    &any_key_matches) == MatchResult::no_match);
  CHECK(match_get_any_keys(expr, parse_sequence("+B"),
    &any_key_matches) == MatchResult::might_match);
  CHECK(match_get_any_keys(expr, parse_sequence("+B -B"),
    &any_key_matches) == MatchResult::no_match);
  CHECK(match_get_any_keys(expr, parse_sequence("+B +A"),
    &any_key_matches) == MatchResult::match);
  CHECK(format_list(any_key_matches) == "A");

  REQUIRE_NOTHROW(expr = parse_input("B Any"));
  CHECK(match_get_any_keys(expr, parse_sequence("+A"),
    &any_key_matches) == MatchResult::no_match);
  CHECK(match_get_any_keys(expr, parse_sequence("+B"),
    &any_key_matches) == MatchResult::might_match);
  CHECK(match_get_any_keys(expr, parse_sequence("+B -B"),
    &any_key_matches) == MatchResult::might_match);
  CHECK(match_get_any_keys(expr, parse_sequence("+B -B +A"),
    &any_key_matches) == MatchResult::match);
  CHECK(format_list(any_key_matches) == "A");

  REQUIRE_NOTHROW(expr = parse_input("Any B"));
  CHECK(match(expr, parse_sequence("+A")) == MatchResult::might_match);
  CHECK(match(expr, parse_sequence("+B")) == MatchResult::might_match);
  CHECK(match(expr, parse_sequence("+B -B")) == MatchResult::might_match);
  CHECK(match_get_any_keys(expr, parse_sequence("+B -B +B"),
    &any_key_matches) == MatchResult::match);
  CHECK(format_list(any_key_matches) == "B");
  CHECK(match(expr, parse_sequence("+B -B +C")) == MatchResult::no_match);

  REQUIRE_NOTHROW(expr = parse_input("Any{B}"));
  CHECK(match(expr, parse_sequence("+A")) == MatchResult::might_match);
  CHECK(match(expr, parse_sequence("+B")) == MatchResult::might_match);
  CHECK(match(expr, parse_sequence("+B -B")) == MatchResult::no_match);
  CHECK(match_get_any_keys(expr, parse_sequence("+A +B"),
    &any_key_matches) == MatchResult::match);
  CHECK(format_list(any_key_matches) == "A");

  // Any only matches keyboard keys
  REQUIRE_NOTHROW(expr = parse_input("Any"));
  CHECK(match(expr, parse_sequence("+ButtonLeft")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+WheelUp")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+Virtual1")) == MatchResult::no_match);
}

//--------------------------------------------------------------------

TEST_CASE("Match Timeout", "[MatchKeySequence]") {
  auto expr = KeySequence();
  auto input_timeout_event = KeyEvent{ };

  REQUIRE_NOTHROW(expr = parse_input("A{100ms}"));
  CHECK(match_get_timeout_event(expr, parse_sequence("+A"),
    &input_timeout_event) == MatchResult::might_match);
  CHECK(input_timeout_event == make_timeout_ms(100, true));
  input_timeout_event = KeyEvent{ };

  CHECK(match_get_timeout_event(expr,
    KeySequence{ KeyEvent{ Key::A, KeyState::Down },
                 reply_timeout_ms(100) },
    &input_timeout_event) == MatchResult::match);
  CHECK(input_timeout_event == KeyEvent{ });

  CHECK(match_get_timeout_event(expr, 
    KeySequence{ KeyEvent{ Key::A, KeyState::Down },
                 reply_timeout_ms(71) },
    &input_timeout_event) == MatchResult::no_match);
  CHECK(input_timeout_event == KeyEvent{ });
}

//--------------------------------------------------------------------

TEST_CASE("Match Not Timeout", "[MatchKeySequence]") {
  auto expr = KeySequence();
  auto input_timeout_event = KeyEvent{ };

  REQUIRE_NOTHROW(expr = parse_input("A{!100ms}"));
  CHECK(match_get_timeout_event(expr, parse_sequence("+A"),
    &input_timeout_event) == MatchResult::might_match);
  CHECK(input_timeout_event == make_not_timeout_ms(100, true));
  input_timeout_event = KeyEvent{ };

  // cancelled
  CHECK(match_get_timeout_event(expr,
    KeySequence{ KeyEvent{ Key::A, KeyState::Down },
                 reply_timeout_ms(100) },
    &input_timeout_event) == MatchResult::no_match);
  CHECK(input_timeout_event == KeyEvent{ });

  CHECK(match_get_timeout_event(expr, 
    KeySequence{ KeyEvent{ Key::A, KeyState::Down },
                 reply_timeout_ms(71) },
    &input_timeout_event) == MatchResult::might_match);
  CHECK(input_timeout_event == KeyEvent{ });

  // cancelled by release
  CHECK(match_get_timeout_event(expr, 
    KeySequence{ KeyEvent{ Key::A, KeyState::Down },
                 reply_timeout_ms(71),
                 KeyEvent{ Key::A, KeyState::Up } },
    &input_timeout_event) == MatchResult::match);
  CHECK(input_timeout_event == KeyEvent{ });

  // cancelled by another key
  CHECK(match_get_timeout_event(expr, 
    KeySequence{ KeyEvent{ Key::A, KeyState::Down },
                 reply_timeout_ms(71),
                 KeyEvent{ Key::B, KeyState::Down } },
    &input_timeout_event) == MatchResult::no_match);
  CHECK(input_timeout_event == KeyEvent{ });

  // suppressed timeout
  CHECK(match_get_timeout_event(expr, 
    KeySequence{ KeyEvent{ Key::A, KeyState::Down },
                 KeyEvent{ Key::A, KeyState::Up } },
    &input_timeout_event) == MatchResult::no_match);
  CHECK(input_timeout_event == KeyEvent{ });
}

//--------------------------------------------------------------------

TEST_CASE("Ignore surplus Timeouts", "[MatchKeySequence]") {
  auto expr = KeySequence();

  REQUIRE_NOTHROW(expr = parse_input("A{B}"));
  CHECK(match(expr,
    KeySequence{ KeyEvent{ Key::A, KeyState::Down },
                 KeyEvent{ Key::B, KeyState::Down }
    }) == MatchResult::match);

  CHECK(match(expr,
    KeySequence{ KeyEvent{ Key::A, KeyState::Down },
                 KeyEvent{ Key::B, KeyState::Down },
                 reply_timeout_ms(100)
    }) == MatchResult::match);

  CHECK(match(expr,
    KeySequence{ KeyEvent{ Key::A, KeyState::Down },
                 reply_timeout_ms(100),
                 KeyEvent{ Key::B, KeyState::Down }
    }) == MatchResult::match);
}

//--------------------------------------------------------------------
