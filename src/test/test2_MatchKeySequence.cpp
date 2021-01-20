
#include "test.h"
#include "runtime/MatchKeySequence.h"

namespace  {
  MatchResult match(const KeySequence& expression,
      const KeySequence& sequence) {
    static auto match = MatchKeySequence();
    return match(expression, sequence);
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

  REQUIRE_NOTHROW(expr = parse_input("B !A C"));
  CHECK(match(expr, parse_sequence("+A")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+A +B")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+A +B +C")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+B")) == MatchResult::might_match);
  CHECK(match(expr, parse_sequence("+B +C")) == MatchResult::match);

  REQUIRE_NOTHROW(expr = parse_input("B C !A"));
  CHECK(match(expr, parse_sequence("+A")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+A +B")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+A +B +C")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+B")) == MatchResult::might_match);
  CHECK(match(expr, parse_sequence("+B +C")) == MatchResult::match);

  REQUIRE_NOTHROW(expr = parse_input("B{!A C}"));
  CHECK(match(expr, parse_sequence("+A")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+A +B")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+A +B +C")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+B")) == MatchResult::might_match);
  CHECK(match(expr, parse_sequence("+B +C")) == MatchResult::match);
}

//--------------------------------------------------------------------

TEST_CASE("Match ANY", "[MatchKeySequence]") {
  auto expr = KeySequence();

  REQUIRE_NOTHROW(expr = parse_input("Any"));
  CHECK(match(expr, parse_sequence("+A")) == MatchResult::match);

  REQUIRE_NOTHROW(expr = parse_input("Any B"));
  CHECK(match(expr, parse_sequence("+A")) == MatchResult::might_match);
  CHECK(match(expr, parse_sequence("+B")) == MatchResult::might_match);
  CHECK(match(expr, parse_sequence("+B -B")) == MatchResult::might_match);
  CHECK(match(expr, parse_sequence("+B -B +B")) == MatchResult::match);
  CHECK(match(expr, parse_sequence("+B -B +C")) == MatchResult::no_match);

  REQUIRE_NOTHROW(expr = parse_input("Any{B}"));
  CHECK(match(expr, parse_sequence("+A")) == MatchResult::might_match);
  CHECK(match(expr, parse_sequence("+B")) == MatchResult::might_match);
  CHECK(match(expr, parse_sequence("+B -B")) == MatchResult::no_match);
  CHECK(match(expr, parse_sequence("+A +B")) == MatchResult::match);
}

//--------------------------------------------------------------------
