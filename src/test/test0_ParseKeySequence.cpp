
#include "test.h"

TEST_CASE("Input Expression", "[ParseKeySequence]") {
  // Empty
  CHECK_THROWS(parse_input(""));

  // A has to be pressed.
  // "A"  =>  +A ~A
  CHECK(parse_input("A") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::A, KeyState::UpAsync),
  }));

  // A has to be pressed first then B. A can still be hold.
  // "A B"  =>  +A ~A +B ~B
  CHECK(parse_input("A B") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::A, KeyState::UpAsync),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::B, KeyState::UpAsync),
  }));

  // A has to be pressed first then B. A must not be released in between.
  // "A{B}"  =>  +A +B ~B ~A
  CHECK(parse_input("A{B}") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::B, KeyState::UpAsync),
    KeyEvent(Key::A, KeyState::UpAsync),
  }));
  CHECK_THROWS(parse_input("{B}"));

  // A has to be pressed first then B, then C. None must be released in between.
  // "A{B{C}}"  =>  +A +B +C ~C ~B ~A
  CHECK(parse_input("A{B{C}}") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::C, KeyState::Down),
    KeyEvent(Key::C, KeyState::UpAsync),
    KeyEvent(Key::B, KeyState::UpAsync),
    KeyEvent(Key::A, KeyState::UpAsync),
  }));

  // A and B have to be pressed together, order does not matter.
  // "(A B)"  =>  *A *B +A +B ~A ~B
  CHECK(parse_input("(A B)") == (KeySequence{
    KeyEvent(Key::A, KeyState::DownAsync),
    KeyEvent(Key::B, KeyState::DownAsync),
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::A, KeyState::UpAsync),
    KeyEvent(Key::B, KeyState::UpAsync),
  }));
  CHECK_THROWS(parse_input("(A B{C})"));

  // A has to be pressed first then B and C together. A can be released any time.
  // "A(B C)"  =>  +A ~A *B *C +B +C ~B ~C
  CHECK(parse_input("A(B C)") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::A, KeyState::UpAsync),
    KeyEvent(Key::B, KeyState::DownAsync),
    KeyEvent(Key::C, KeyState::DownAsync),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::C, KeyState::Down),
    KeyEvent(Key::B, KeyState::UpAsync),
    KeyEvent(Key::C, KeyState::UpAsync),
  }));

  // A has to be pressed first then B, then C. A has to be released last.
  // "A{B C}"  =>  +A +B ~B +C ~C ~A
  CHECK(parse_input("A{B C}") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::B, KeyState::UpAsync),
    KeyEvent(Key::C, KeyState::Down),
    KeyEvent(Key::C, KeyState::UpAsync),
    KeyEvent(Key::A, KeyState::UpAsync),
  }));

  // A has to be pressed first then B and C together. A has to be released last.
  // "A{(B C)}"  =>  +A *B *C +B +C ~B ~C ~A
  CHECK(parse_input("A{(B C)}") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::B, KeyState::DownAsync),
    KeyEvent(Key::C, KeyState::DownAsync),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::C, KeyState::Down),
    KeyEvent(Key::B, KeyState::UpAsync),
    KeyEvent(Key::C, KeyState::UpAsync),
    KeyEvent(Key::A, KeyState::UpAsync),
  }));

  // "A{B{C} D{E}}"  =>  +A +B +C ~C ~B +D +D ~E ~E ~A
  CHECK(parse_input("A{B{C} D{E}}") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::C, KeyState::Down),
    KeyEvent(Key::C, KeyState::UpAsync),
    KeyEvent(Key::B, KeyState::UpAsync),
    KeyEvent(Key::D, KeyState::Down),
    KeyEvent(Key::E, KeyState::Down),
    KeyEvent(Key::E, KeyState::UpAsync),
    KeyEvent(Key::D, KeyState::UpAsync),
    KeyEvent(Key::A, KeyState::UpAsync),
  }));

  // "A B{C} D"  =>  +A ~A +B +C ~C ~B +D ~D
  CHECK(parse_input("A B{C} D") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::A, KeyState::UpAsync),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::C, KeyState::Down),
    KeyEvent(Key::C, KeyState::UpAsync),
    KeyEvent(Key::B, KeyState::UpAsync),
    KeyEvent(Key::D, KeyState::Down),
    KeyEvent(Key::D, KeyState::UpAsync),
  }));

  // "A{(B C) D}"  =>  +A *B *C +B +C ~B ~C +D ~D ~A
  CHECK(parse_input("A{(B C) D}") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::B, KeyState::DownAsync),
    KeyEvent(Key::C, KeyState::DownAsync),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::C, KeyState::Down),
    KeyEvent(Key::B, KeyState::UpAsync),
    KeyEvent(Key::C, KeyState::UpAsync),
    KeyEvent(Key::D, KeyState::Down),
    KeyEvent(Key::D, KeyState::UpAsync),
    KeyEvent(Key::A, KeyState::UpAsync),
  }));

  // A and B have to be pressed together, order does not matter. Then C, then D.
  // "(A B){C D}"  =>  *A *B +A +B +C ~C +D ~D ~B ~A
  CHECK(parse_input("(A B){C D}") == (KeySequence{
    KeyEvent(Key::A, KeyState::DownAsync),
    KeyEvent(Key::B, KeyState::DownAsync),
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::C, KeyState::Down),
    KeyEvent(Key::C, KeyState::UpAsync),
    KeyEvent(Key::D, KeyState::Down),
    KeyEvent(Key::D, KeyState::UpAsync),
    KeyEvent(Key::B, KeyState::UpAsync),
    KeyEvent(Key::A, KeyState::UpAsync),
  }));

  // Not
  CHECK(parse_input("A !B") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::A, KeyState::UpAsync),
    KeyEvent(Key::B, KeyState::Not),
  }));

  // Not as Up
  CHECK(parse_input("A !A B !B") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::A, KeyState::Up),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::B, KeyState::Up),
  }));

  CHECK(parse_input("A B !A !B") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::A, KeyState::UpAsync),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::B, KeyState::UpAsync),
    KeyEvent(Key::A, KeyState::Up),
    KeyEvent(Key::B, KeyState::Up),
  }));

  CHECK(parse_input("A !A B") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::A, KeyState::Up),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::B, KeyState::UpAsync),
  }));

  CHECK(parse_input("!A B A !A") == (KeySequence{
    KeyEvent(Key::A, KeyState::Not),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::B, KeyState::UpAsync),
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::A, KeyState::Up),
  }));

  CHECK(parse_input("A{B !B}") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::B, KeyState::Up),
    KeyEvent(Key::A, KeyState::UpAsync),
  }));

  CHECK(parse_input("A{B !B} !A") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::B, KeyState::Up),
    KeyEvent(Key::A, KeyState::Up),
  }));

  CHECK(parse_input("A{B} !A") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::B, KeyState::UpAsync),
    KeyEvent(Key::A, KeyState::Up),
  }));

  CHECK_THROWS(parse_input("!"));
  CHECK_THROWS(parse_input("!A"));
  CHECK_THROWS(parse_input("!(A B)"));
  CHECK_THROWS(parse_input("!A{B}"));
  CHECK_THROWS(parse_input("A{!B}"));
  CHECK_THROWS(parse_input("!A 100ms"));

  // Output on release
  CHECK_THROWS(parse_input("A ^ B"));

  // No MightMatch
  CHECK(parse_input("? A B") == (KeySequence{
    KeyEvent(Key::none, KeyState::NoMightMatch),
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::A, KeyState::UpAsync),
    KeyEvent(Key::B, KeyState::Down),
  }));

  CHECK_THROWS(parse_input("?"));
  CHECK_THROWS(parse_input("? ? A B"));
  CHECK_THROWS(parse_input("A ? B"));
  CHECK_THROWS(parse_input("A ?"));
  CHECK_THROWS(parse_input("(?A)"));
  CHECK_THROWS(parse_input("? A{100ms}"));
  CHECK_THROWS(parse_input("? Virtual1{A}"));
  
  // Any
  CHECK_THROWS(parse_input("!Any"));
  CHECK_NOTHROW(parse_input("Any"));
  CHECK_NOTHROW(parse_input("A Any"));
  CHECK_NOTHROW(parse_input("A !Any B"));
  CHECK_NOTHROW(parse_input("? Any A"));
  CHECK_NOTHROW(parse_input("? !B Any A"));
  CHECK_NOTHROW(parse_input("? A Any"));
  CHECK_NOTHROW(parse_input("? A !Any B"));

  // Virtual
  CHECK(parse_input("Virtual0") == (KeySequence{
    KeyEvent(Key::first_virtual, KeyState::Down),
    KeyEvent(Key::first_virtual, KeyState::UpAsync),
  }));
  CHECK(parse_input("!Virtual0 A") == (KeySequence{
    KeyEvent(Key::first_virtual, KeyState::Not),
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::A, KeyState::UpAsync),
  }));

  // Timeout
  CHECK(parse_input("A 1000ms") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::A, KeyState::UpAsync),
    make_timeout_ms(1000, false),
  }));

  CHECK(parse_input("A !A 1000ms") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::A, KeyState::Up),
    make_timeout_ms(1000, false),
  }));

  CHECK(parse_input("A{1000ms}") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    make_timeout_ms(1000, true),
    KeyEvent(Key::A, KeyState::UpAsync),
  }));

  CHECK(parse_input("(A B){1000ms}") == (KeySequence{
    KeyEvent(Key::A, KeyState::DownAsync),
    KeyEvent(Key::B, KeyState::DownAsync),
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::B, KeyState::Down),
    make_timeout_ms(1000, true),
    KeyEvent(Key::B, KeyState::UpAsync),
    KeyEvent(Key::A, KeyState::UpAsync),
  }));

  CHECK(parse_input("A 1000ms B") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::A, KeyState::UpAsync),
    make_timeout_ms(1000, false),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::B, KeyState::UpAsync),
  }));

  CHECK(parse_input("A !A 1000ms B") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::A, KeyState::Up),
    make_timeout_ms(1000, false),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::B, KeyState::UpAsync),
  }));

  CHECK(parse_input("A{1000ms B}") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    make_timeout_ms(1000, true),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::B, KeyState::UpAsync),
    KeyEvent(Key::A, KeyState::UpAsync),
  }));

  CHECK_NOTHROW(parse_input("A 10000000ms"));
  CHECK_THROWS(parse_input("1000ms A"));
  CHECK_THROWS(parse_input("(A 1000ms)"));
  CHECK_THROWS(parse_input("(A !1000ms)"));

  // Not Timeout
  CHECK(parse_input("A !1000ms") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::A, KeyState::UpAsync),
    make_not_timeout_ms(1000, false),
  }));

  CHECK(parse_input("A !A !1000ms") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::A, KeyState::Up),
    make_not_timeout_ms(1000, false),
  }));

  CHECK(parse_input("A{!1000ms}") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    make_not_timeout_ms(1000, true),
    KeyEvent(Key::A, KeyState::UpAsync),
    KeyEvent(Key::A, KeyState::Up),
  }));

  CHECK(parse_input("(A B){!1000ms}") == (KeySequence{
    KeyEvent(Key::A, KeyState::DownAsync),
    KeyEvent(Key::B, KeyState::DownAsync),
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::B, KeyState::Down),
    make_not_timeout_ms(1000, true),
    KeyEvent(Key::B, KeyState::UpAsync),
    KeyEvent(Key::A, KeyState::UpAsync),
    KeyEvent(Key::B, KeyState::Up),
    KeyEvent(Key::A, KeyState::Up), // <- unexpected that both need to be released
  }));

  CHECK(parse_input("A{B{!1000ms}}") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::B, KeyState::Down),
    make_not_timeout_ms(1000, true),
    KeyEvent(Key::B, KeyState::UpAsync),
    KeyEvent(Key::B, KeyState::Up),
    KeyEvent(Key::A, KeyState::UpAsync),
  }));

  CHECK(parse_input("A{B !1000ms}") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::B, KeyState::UpAsync),
    make_not_timeout_ms(1000, true),
    KeyEvent(Key::A, KeyState::UpAsync),
    KeyEvent(Key::A, KeyState::Up),
  }));

  CHECK(parse_input("A{1000ms !1000ms}") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    make_timeout_ms(1000, true),
    make_not_timeout_ms(1000, true),
    KeyEvent(Key::A, KeyState::UpAsync),
    KeyEvent(Key::A, KeyState::Up),
  }));

  CHECK(parse_input("(A B){1000ms !1000ms}") == (KeySequence{
    KeyEvent(Key::A, KeyState::DownAsync),
    KeyEvent(Key::B, KeyState::DownAsync),
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::B, KeyState::Down),
    make_timeout_ms(1000, true),
    make_not_timeout_ms(1000, true),
    KeyEvent(Key::B, KeyState::UpAsync),
    KeyEvent(Key::A, KeyState::UpAsync),
    KeyEvent(Key::B, KeyState::Up),
    KeyEvent(Key::A, KeyState::Up), // <- unexpected that both need to be released
  }));

  // Timeouts are merged to minimize undefined behaviour
  CHECK(parse_input("A{1000ms 100ms !1000ms !100ms !10ms 1000ms}") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    make_timeout_ms(1100, true),
    make_not_timeout_ms(1110, true),
    make_timeout_ms(1000, true),
    KeyEvent(Key::A, KeyState::UpAsync),
  }));
}

//--------------------------------------------------------------------

TEST_CASE("Output Expression", "[ParseKeySequence]") {
  // Empty
  CHECK(parse_output("") == (KeySequence{ }));

  // Press A.
  // "A"  =>  +A
  CHECK(parse_output("A") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
  }));

  // Press A and then B.
  // "A B"  =>  +A -A +B -B
  CHECK(parse_output("A B") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::A, KeyState::Up),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::B, KeyState::Up),
  }));

  // Press A and keep hold while pressing B.
  //   "A{B}"  =>  +A +B -B -A
  CHECK(parse_output("A{B}") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::B, KeyState::Up),
    KeyEvent(Key::A, KeyState::Up),
  }));
  CHECK_THROWS(parse_output("{B}"));

  // Press A and B together, order does not matter.
  //   "(A B)"  =>  +A +B
  CHECK(parse_output("(A B)") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::B, KeyState::Down),
  }));

  // Press A, B, C together, order does not matter.
  //   "(A B C)"  =>  +A +B +C
  CHECK(parse_output("(A B C)") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::C, KeyState::Down),
  }));

  // Press A first and then B and C, order does not matter.
  // "A(B C)"  =>  +A -A +B +C -B -C
  CHECK(parse_output("A(B C)") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::A, KeyState::Up),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::C, KeyState::Down),
    KeyEvent(Key::B, KeyState::Up),
    KeyEvent(Key::C, KeyState::Up),
  }));

  // Press A and keep hold while pressing B and then C.
  // "A{B C}"  =>  +A +B B- +C -C -A
  CHECK(parse_output("A{B C}") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::B, KeyState::Up),
    KeyEvent(Key::C, KeyState::Down),
    KeyEvent(Key::C, KeyState::Up),
    KeyEvent(Key::A, KeyState::Up),
  }));

  // Press A and keep hold while pressing B and C, order does not matter.
  // "A{(B C)}"  =>  +A +B +C -C -B -A
  CHECK(parse_output("A{(B C)}") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::C, KeyState::Down),
    KeyEvent(Key::B, KeyState::Up),
    KeyEvent(Key::C, KeyState::Up),
    KeyEvent(Key::A, KeyState::Up),
  }));

  // Press A and B together, order does not matter,
  // keep hold while pressing C and then D.
  // "(A B){C D}"  =>  +A +B +C -C +D -D -B -A
  CHECK(parse_output("(A B){C D}") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::C, KeyState::Down),
    KeyEvent(Key::C, KeyState::Up),
    KeyEvent(Key::D, KeyState::Down),
    KeyEvent(Key::D, KeyState::Up),
    KeyEvent(Key::B, KeyState::Up),
    KeyEvent(Key::A, KeyState::Up),
  }));

  // Press A, B and C together.
  // "A{B{C}}"  =>  +A +B +C -C -B -A
  CHECK(parse_output("A{B{C}}") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::C, KeyState::Down),
    KeyEvent(Key::C, KeyState::Up),
    KeyEvent(Key::B, KeyState::Up),
    KeyEvent(Key::A, KeyState::Up),
  }));

  // Not
  CHECK(parse_output("!A") == (KeySequence{
    KeyEvent(Key::A, KeyState::Not),
  }));

  CHECK(parse_output("A !A") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::A, KeyState::Not),
  }));

  CHECK(parse_output("A{B !B}") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::B, KeyState::Not),
    KeyEvent(Key::A, KeyState::Up),
  }));

  // Output on release
  CHECK(parse_output("A ^ B") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::A, KeyState::Up),
    KeyEvent(Key::none, KeyState::OutputOnRelease),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::B, KeyState::Up),
  }));
  CHECK(parse_output("^ A B") == (KeySequence{
    KeyEvent(Key::none, KeyState::OutputOnRelease),
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::A, KeyState::Up),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::B, KeyState::Up),
  }));
  CHECK(parse_output("A B ^") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::A, KeyState::Up),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::B, KeyState::Up),
    KeyEvent(Key::none, KeyState::OutputOnRelease),
  }));
  CHECK(parse_output("^") == (KeySequence{
    KeyEvent(Key::none, KeyState::OutputOnRelease),
  }));
  CHECK_THROWS(parse_output("A ^ B ^ C"));
  CHECK_THROWS(parse_output("^ A ^ B"));
  CHECK_THROWS(parse_output("(A ^ B)"));
  CHECK_THROWS(parse_output("A{^ B}"));
  CHECK_THROWS(parse_output("A^{B}"));

  // No MightMatch
  CHECK_THROWS(parse_output("? A B"));

  // Any
  CHECK(parse_output("Any") == (KeySequence{
    KeyEvent(Key::any, KeyState::Down),
  }));
  CHECK(parse_output("!Any") == (KeySequence{
    KeyEvent(Key::any, KeyState::Not),
  }));

  // Virtual
  CHECK(parse_output("Virtual0") == (KeySequence{
    KeyEvent(Key::first_virtual, KeyState::Down),
  }));
  CHECK(parse_output("!Virtual0") == (KeySequence{
    KeyEvent(Key::first_virtual, KeyState::Not),
  }));
  CHECK_NOTHROW(parse_output("Virtual255") == (KeySequence{
    KeyEvent(static_cast<Key>(*Key::first_virtual + 255), KeyState::Down),
  }));
  CHECK_THROWS(parse_output("Virtual256"));

  // Timeout
  CHECK(parse_output("A 1000ms") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::A, KeyState::Up),
    make_output_timeout_ms(1000),
  }));

  CHECK(parse_output("A{1000ms}") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    make_output_timeout_ms(1000),
    KeyEvent(Key::A, KeyState::Up),
  }));

  CHECK(parse_output("A 1000ms B") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::A, KeyState::Up),
    make_output_timeout_ms(1000),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::B, KeyState::Up),
  }));

  CHECK(parse_output("A{1000ms B}") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    make_output_timeout_ms(1000),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::B, KeyState::Up),
    KeyEvent(Key::A, KeyState::Up),
  }));

  CHECK(parse_output("1000ms A") == (KeySequence{
    make_output_timeout_ms(1000),
    KeyEvent(Key::A, KeyState::Down),
  }));

  CHECK(parse_output("(A B){1000ms}") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::B, KeyState::Down),
    make_output_timeout_ms(1000),
    KeyEvent(Key::B, KeyState::Up),
    KeyEvent(Key::A, KeyState::Up),
  }));

  CHECK_NOTHROW(parse_output("A 10000000ms"));

  // Not Timeouts are not allowed
  CHECK_THROWS(parse_output("(A !1000ms)"));
  CHECK_THROWS(parse_output("A !1000ms"));
  CHECK_THROWS(parse_output("A{!1000ms}"));
  CHECK_THROWS(parse_output("A{1000ms !1000ms}"));

  // Timeouts are merged to minimize undefined behaviour
  CHECK(parse_output("A{1000ms 100ms 1000ms}") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    make_output_timeout_ms(2100),
    KeyEvent(Key::A, KeyState::Up),
  }));
}

//--------------------------------------------------------------------

TEST_CASE("Key names", "[ParseKeySequence]") {
  CHECK(parse_output("(Digit1 2 KeyA B Any)") == (KeySequence{
    KeyEvent(Key::Digit1, KeyState::Down),
    KeyEvent(Key::Digit2, KeyState::Down),
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::any, KeyState::Down),
  }));

  CHECK(parse_output("(OSLeft MetaLeft OSRight MetaRight)") == (KeySequence{
    KeyEvent(Key::MetaLeft, KeyState::Down),
    KeyEvent(Key::MetaLeft, KeyState::Down),
    KeyEvent(Key::MetaRight, KeyState::Down),
    KeyEvent(Key::MetaRight, KeyState::Down),
  }));

  CHECK(parse_output("(Virtual0 Virtual255)") == (KeySequence{
    KeyEvent(Key::first_virtual, KeyState::Down),
    KeyEvent(static_cast<Key>(static_cast<int>(Key::first_virtual) + 255),
      KeyState::Down),
  }));
  CHECK_THROWS(parse_output("Virtual256"));
  CHECK_THROWS(parse_output("Key1"));
  CHECK_THROWS(parse_output("DigitA"));
}

//--------------------------------------------------------------------

TEST_CASE("Key code", "[ParseKeySequence]") {
  CHECK(parse_output("1ms (0 9 F 01 61439 0x1 0XefFF)") == (KeySequence{
    make_output_timeout_ms(1),
    KeyEvent(Key::Digit0, KeyState::Down),
    KeyEvent(Key::Digit9, KeyState::Down),
    KeyEvent(Key::F, KeyState::Down),
    KeyEvent(static_cast<Key>(1), KeyState::Down),
    KeyEvent(static_cast<Key>(61439), KeyState::Down),
    KeyEvent(static_cast<Key>(0x1), KeyState::Down),
    KeyEvent(static_cast<Key>(0xEFFF), KeyState::Down),
  }));

  CHECK_THROWS(parse_output("00"));
  CHECK_THROWS(parse_output("0F"));
  CHECK_THROWS(parse_output("-01"));
  CHECK_THROWS(parse_output("61440"));
  CHECK_THROWS(parse_output("0xF000"));
  CHECK_THROWS(parse_output("01z"));
  CHECK_THROWS(parse_output("0xEFz"));
  CHECK_THROWS(parse_output("0x1ms"));
  CHECK_THROWS(parse_output("0xEFms"));
}

//--------------------------------------------------------------------

TEST_CASE("Input string", "[ParseKeySequence]") {
  CHECK(parse_input("'a'") == (KeySequence{
    KeyEvent(Key::Shift, KeyState::Not),
    KeyEvent(Key::AltLeft, KeyState::Not),
    KeyEvent(Key::AltRight, KeyState::Not),
    KeyEvent(Key::Control, KeyState::Not),
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::A, KeyState::UpAsync),
  }));

  CHECK(parse_input("A 'b' C") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::A, KeyState::UpAsync),
    KeyEvent(Key::Shift, KeyState::Not),
    KeyEvent(Key::AltLeft, KeyState::Not),
    KeyEvent(Key::AltRight, KeyState::Not),
    KeyEvent(Key::Control, KeyState::Not),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::B, KeyState::UpAsync),
    KeyEvent(Key::C, KeyState::Down),
    KeyEvent(Key::C, KeyState::UpAsync),
  }));

  CHECK(parse_input("'Cat'") == (KeySequence{
    KeyEvent(Key::Shift, KeyState::DownAsync),
    KeyEvent(Key::Shift, KeyState::Down),
    KeyEvent(Key::AltLeft, KeyState::Not),
    KeyEvent(Key::AltRight, KeyState::Not),
    KeyEvent(Key::Control, KeyState::Not),
    KeyEvent(Key::C, KeyState::Down),
    KeyEvent(Key::C, KeyState::UpAsync),
    KeyEvent(Key::Shift, KeyState::Up),
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::A, KeyState::UpAsync),
    KeyEvent(Key::T, KeyState::Down),
    KeyEvent(Key::T, KeyState::UpAsync),
  }));

  CHECK(parse_input("'cAt'") == (KeySequence{
    KeyEvent(Key::Shift, KeyState::Not),
    KeyEvent(Key::AltLeft, KeyState::Not),
    KeyEvent(Key::AltRight, KeyState::Not),
    KeyEvent(Key::Control, KeyState::Not),
    KeyEvent(Key::C, KeyState::Down),
    KeyEvent(Key::C, KeyState::UpAsync),
    KeyEvent(Key::Shift, KeyState::DownAsync),
    KeyEvent(Key::Shift, KeyState::Down),
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::A, KeyState::UpAsync),
    KeyEvent(Key::Shift, KeyState::Up),
    KeyEvent(Key::T, KeyState::Down),
    KeyEvent(Key::T, KeyState::UpAsync),
  }));

  CHECK(parse_input("'CAP'") == (KeySequence{
    KeyEvent(Key::Shift, KeyState::DownAsync),
    KeyEvent(Key::Shift, KeyState::Down),
    KeyEvent(Key::AltLeft, KeyState::Not),
    KeyEvent(Key::AltRight, KeyState::Not),
    KeyEvent(Key::Control, KeyState::Not),
    KeyEvent(Key::C, KeyState::Down),
    KeyEvent(Key::C, KeyState::UpAsync),
    KeyEvent(Key::Shift, KeyState::UpAsync),
    KeyEvent(Key::Shift, KeyState::DownAsync),
    KeyEvent(Key::Shift, KeyState::Down),
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::A, KeyState::UpAsync),
    KeyEvent(Key::Shift, KeyState::UpAsync),
    KeyEvent(Key::Shift, KeyState::DownAsync),
    KeyEvent(Key::Shift, KeyState::Down),
    KeyEvent(Key::P, KeyState::Down),
    KeyEvent(Key::P, KeyState::UpAsync),
    KeyEvent(Key::Shift, KeyState::UpAsync),
  }));
  
  CHECK(parse_input("A{'b'}") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::B, KeyState::UpAsync),
    KeyEvent(Key::A, KeyState::UpAsync),
  }));
  
  CHECK(parse_input("A{'Bb'}") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::B, KeyState::Down), // no modifiers are checked
    KeyEvent(Key::B, KeyState::UpAsync),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::B, KeyState::UpAsync),
    KeyEvent(Key::A, KeyState::UpAsync),
  }));

  CHECK(parse_input("('A' 'b' '')") == (KeySequence{
    KeyEvent(Key::A, KeyState::DownAsync),
    KeyEvent(Key::B, KeyState::DownAsync),
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::A, KeyState::UpAsync),
    KeyEvent(Key::B, KeyState::UpAsync),
  }));

  CHECK(parse_input("'A'{'B'{'C'}}") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::C, KeyState::Down),
    KeyEvent(Key::C, KeyState::UpAsync),
    KeyEvent(Key::B, KeyState::UpAsync),
    KeyEvent(Key::A, KeyState::UpAsync),
  }));

  CHECK_THROWS(parse_input("('Aa' 'Bb')"));
  CHECK_THROWS(parse_input("'Aa'{ A })"));
  CHECK_THROWS(parse_input("''{ A }"));
}

//--------------------------------------------------------------------

TEST_CASE("Output string", "[ParseKeySequence]") {
  CHECK(parse_output("'a'") == (KeySequence{
    KeyEvent(Key::Shift, KeyState::Not),
    KeyEvent(Key::A, KeyState::Down),
  }));

  CHECK(parse_output("A 'b' C") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::A, KeyState::Up),
    KeyEvent(Key::Shift, KeyState::Not),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::B, KeyState::Up),
    KeyEvent(Key::C, KeyState::Down),
    KeyEvent(Key::C, KeyState::Up),
  }));

  CHECK(parse_output("\"ab\"") == (KeySequence{
    KeyEvent(Key::any, KeyState::Not),
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::A, KeyState::Up),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::B, KeyState::Up),
  }));

  // single characters only suppress Shift
  // to make shortcuts like Ctrl-/ work when mapping to '/'
  CHECK(parse_output("'a'") == (KeySequence{
    KeyEvent(Key::Shift, KeyState::Not),
    KeyEvent(Key::A, KeyState::Down),
  }));
    
  CHECK(parse_output("'A'") == (KeySequence{
    KeyEvent(Key::Shift, KeyState::Down),
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::A, KeyState::Up),
    KeyEvent(Key::Shift, KeyState::Up),
  }));

  CHECK(parse_output("A 'B' C") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::A, KeyState::Up),
    KeyEvent(Key::Shift, KeyState::Down),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::B, KeyState::Up),
    KeyEvent(Key::Shift, KeyState::Up),
    KeyEvent(Key::C, KeyState::Down),
    KeyEvent(Key::C, KeyState::Up),
  }));

  CHECK(parse_output("\"AB\"") == (KeySequence{
    KeyEvent(Key::any, KeyState::Not),
    KeyEvent(Key::Shift, KeyState::Down),
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::A, KeyState::Up),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::B, KeyState::Up),
    KeyEvent(Key::Shift, KeyState::Up),
  }));

  CHECK(parse_output("'AbC'") == (KeySequence{
    KeyEvent(Key::any, KeyState::Not),
    KeyEvent(Key::Shift, KeyState::Down),
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::A, KeyState::Up),
    KeyEvent(Key::Shift, KeyState::Up),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::B, KeyState::Up),
    KeyEvent(Key::Shift, KeyState::Down),
    KeyEvent(Key::C, KeyState::Down),
    KeyEvent(Key::C, KeyState::Up),
    KeyEvent(Key::Shift, KeyState::Up),
  }));

  CHECK(parse_output("'aB'") == (KeySequence{
    KeyEvent(Key::any, KeyState::Not),
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::A, KeyState::Up),
    KeyEvent(Key::Shift, KeyState::Down),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::B, KeyState::Up),
    KeyEvent(Key::Shift, KeyState::Up),
  }));

  CHECK(parse_output("A{'b'}") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::B, KeyState::Up),
    KeyEvent(Key::A, KeyState::Up),
  }));
  
  CHECK(parse_output("A{'Bc'}") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::Shift, KeyState::Down),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::B, KeyState::Up),
    KeyEvent(Key::Shift, KeyState::Up),
    KeyEvent(Key::C, KeyState::Down),
    KeyEvent(Key::C, KeyState::Up),
    KeyEvent(Key::A, KeyState::Up),
  }));
  
  CHECK(parse_output("('A' 'b' '')") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::B, KeyState::Down),
  }));

  CHECK(parse_output("'A'{'B'{'C'}}") == (KeySequence{
    KeyEvent(Key::A, KeyState::Down),
    KeyEvent(Key::B, KeyState::Down),
    KeyEvent(Key::Shift, KeyState::Down),
    KeyEvent(Key::C, KeyState::Down),
    KeyEvent(Key::C, KeyState::Up),
    KeyEvent(Key::Shift, KeyState::Up),
    KeyEvent(Key::B, KeyState::Up),
    KeyEvent(Key::A, KeyState::Up),
  }));

  CHECK_THROWS(parse_output("'A"));
  CHECK_THROWS(parse_output("A'"));
  CHECK_THROWS(parse_output("'A\""));
  CHECK_THROWS(parse_output("\"A'"));
  CHECK_THROWS(parse_output("('Aa' 'Bb')"));
  CHECK_THROWS(parse_output("'Aa'{ A })"));
  CHECK_THROWS(parse_output("''{ A }"));
  CHECK_THROWS(parse_output("!'B'"));
}

//--------------------------------------------------------------------

TEST_CASE("Parse ContextActive", "[ParseKeySequence]") {
  CHECK(parse_input("ContextActive") == (KeySequence{
    KeyEvent(Key::ContextActive, KeyState::Down),
  }));

  // only allowed in input and alone
  CHECK_THROWS(parse_output("ContextActive"));
  CHECK_THROWS(parse_input("ContextActive A"));
}
