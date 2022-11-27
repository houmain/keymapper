
#include "test.h"

namespace {
  template<size_t N>
  std::string apply_input(Stage& stage, const char(&input)[N], 
                          int device_index = 0) {
    // apply_input all input events and concatenate output
    auto sequence = KeySequence();
    for (auto event : parse_sequence(input))
      for (auto output : stage.update(event, device_index))
        sequence.push_back(output);
    return format_sequence(sequence);
  }

  std::string apply_input(Stage& stage, KeyEvent event,
                          int device_index = 0) {
    return format_sequence(stage.update(event, device_index));
  }
} // namespace

//--------------------------------------------------------------------

TEST_CASE("Simple", "[Stage]") {
  auto config = R"(
    A >> B
  )";
  Stage stage = create_stage(config);

  // A => B
  REQUIRE(apply_input(stage, "+A -A") == "+B -B");

  // B => B
  REQUIRE(apply_input(stage, "+B -B") == "+B -B");

  // +ShiftLeft => +ShiftLeft
  REQUIRE(apply_input(stage, "+ShiftLeft") == "+ShiftLeft");

  // +A +A +A -A => +B +B +B -B
  REQUIRE(apply_input(stage, "+A") == "+B");
  REQUIRE(apply_input(stage, "+A") == "+B");
  REQUIRE(apply_input(stage, "+A") == "+B");
  REQUIRE(apply_input(stage, "-A") == "-B");

  // -LShift => -LShift
  REQUIRE(apply_input(stage, "-ShiftLeft") == "-ShiftLeft");
}

//--------------------------------------------------------------------

TEST_CASE("Layout", "[Stage]") {
  auto config = R"(
    S >> R
    D >> S
    F >> T
  )";
  Stage stage = create_stage(config);

  REQUIRE(apply_input(stage, "+A") == "+A");
  REQUIRE(apply_input(stage, "+S") == "+R");
  REQUIRE(apply_input(stage, "+D") == "+S");
  REQUIRE(apply_input(stage, "+F") == "+T");

  REQUIRE(apply_input(stage, "-A") == "-A");
  REQUIRE(apply_input(stage, "-S") == "-R");
  REQUIRE(apply_input(stage, "-D") == "-S");
  REQUIRE(apply_input(stage, "-F") == "-T");
}

//--------------------------------------------------------------------

TEST_CASE("Layout / Boss Key", "[Stage]") {
  auto config = R"(
    Boss    = ScrollLock
    !Boss S >> R
    !Boss D >> S
    !Boss F >> T
  )";
  Stage stage = create_stage(config);

  REQUIRE(apply_input(stage, "+A") == "+A");
  REQUIRE(apply_input(stage, "+S") == "+R");
  REQUIRE(apply_input(stage, "+D") == "+S");
  REQUIRE(apply_input(stage, "+F") == "+T");

  REQUIRE(apply_input(stage, "-A") == "-A");
  REQUIRE(apply_input(stage, "-S") == "-R");
  REQUIRE(apply_input(stage, "-D") == "-S");
  REQUIRE(apply_input(stage, "-F") == "-T");

  REQUIRE(apply_input(stage, "+ScrollLock") == "+ScrollLock");

  REQUIRE(apply_input(stage, "+A") == "+A");
  REQUIRE(apply_input(stage, "+S") == "+S");
  REQUIRE(apply_input(stage, "+D") == "+D");
  REQUIRE(apply_input(stage, "+F") == "+F");

  REQUIRE(apply_input(stage, "-A") == "-A");
  REQUIRE(apply_input(stage, "-S") == "-S");
  REQUIRE(apply_input(stage, "-D") == "-D");
  REQUIRE(apply_input(stage, "-F") == "-F");

  REQUIRE(apply_input(stage, "-ScrollLock") == "-ScrollLock");
}

//--------------------------------------------------------------------

TEST_CASE("Sequence", "[Stage]") {
  auto config = R"(
    M R >> A
    M S >> B
    R R >> C
    R S >> D
  )";
  Stage stage = create_stage(config);

  // M =>
  REQUIRE(apply_input(stage, "+M") == "");
  REQUIRE(format_sequence(stage.sequence()) == "+M");
  REQUIRE(apply_input(stage, "-M") == "");
  REQUIRE(format_sequence(stage.sequence()) == "+M -M");

  // X => M X
  REQUIRE(apply_input(stage, "+X") == "+M -M +X");
  REQUIRE(format_sequence(stage.sequence()) == "#X");
  REQUIRE(apply_input(stage, "-X") == "-X");
  REQUIRE(stage.is_clear());

  // M M R => M A
  REQUIRE(apply_input(stage, "+M") == "");
  REQUIRE(apply_input(stage, "-M") == "");
  REQUIRE(format_sequence(stage.sequence()) == "+M -M");
  REQUIRE(apply_input(stage, "+M") == "+M -M");
  REQUIRE(format_sequence(stage.sequence()) == "+M");
  REQUIRE(apply_input(stage, "-M") == "");
  REQUIRE(format_sequence(stage.sequence()) == "+M -M");
  REQUIRE(apply_input(stage, "+R") == "+A");
  REQUIRE(apply_input(stage, "-R") == "-A");
  REQUIRE(stage.is_clear());

  // +M S  =>  B
  REQUIRE(apply_input(stage, "+M") == "");
  REQUIRE(apply_input(stage, "+S") == "+B");
  REQUIRE(apply_input(stage, "-S") == "-B");
  REQUIRE(apply_input(stage, "-M") == "");
  REQUIRE(stage.is_clear());

  // S  =>  S
  REQUIRE(apply_input(stage, "+S") == "+S");
  REQUIRE(apply_input(stage, "-S") == "-S");

  // R =>
  REQUIRE(apply_input(stage, "+R") == "");
  REQUIRE(format_sequence(stage.sequence()) == "+R");
  REQUIRE(apply_input(stage, "-R") == "");
  REQUIRE(format_sequence(stage.sequence()) == "+R -R");

  // R => C
  REQUIRE(apply_input(stage, "+R") == "+C");
  REQUIRE(format_sequence(stage.sequence()) == "#R");
  REQUIRE(apply_input(stage, "-R") == "-C");
  REQUIRE(stage.is_clear());
}

//--------------------------------------------------------------------

TEST_CASE("Combo", "[Stage]") {
  auto config = R"(
    Control >> Control
    Control{K} X   >> 1
    Control{K} Any >>
  )";
  Stage stage = create_stage(config);

  // Control{K} X => 1
  REQUIRE(apply_input(stage, "+ControlLeft") == "+ControlLeft");
  REQUIRE(apply_input(stage, "+K") == "");
  REQUIRE(apply_input(stage, "-K") == "");
  REQUIRE(apply_input(stage, "-ControlLeft") == "-ControlLeft");
  REQUIRE(apply_input(stage, "+X") == "+1");
  REQUIRE(apply_input(stage, "-X") == "-1");

  // K => K
  REQUIRE(apply_input(stage, "+K") == "+K");
  REQUIRE(apply_input(stage, "-K") == "-K");

  // X => X
  REQUIRE(apply_input(stage, "+X") == "+X");
  REQUIRE(apply_input(stage, "-X") == "-X");

  // Control{K} Y =>
  REQUIRE(apply_input(stage, "+ControlLeft") == "+ControlLeft");
  REQUIRE(apply_input(stage, "+K -K") == "");
  REQUIRE(apply_input(stage, "-ControlLeft") == "-ControlLeft");
  REQUIRE(apply_input(stage, "+Y -Y") == "");
}

//--------------------------------------------------------------------

TEST_CASE("Filter", "[Stage]") {
  auto config = R"(
    Shift >> Shift
    A >> A
    Any >>
  )";
  Stage stage = create_stage(config);

  // A => A
  REQUIRE(apply_input(stage, "+A") == "+A");
  REQUIRE(format_sequence(stage.sequence()) == "#A");
  REQUIRE(apply_input(stage, "-A") == "-A");
  REQUIRE(stage.is_clear());

  // B C ShiftLeft{D} => ShiftLeft
  REQUIRE(apply_input(stage, "+B -B") == "");
  REQUIRE(stage.is_clear());
  REQUIRE(apply_input(stage, "+C -C") == "");
  REQUIRE(stage.is_clear());
  REQUIRE(apply_input(stage, "+ShiftLeft +D -D -ShiftLeft") == "+ShiftLeft -ShiftLeft");
  REQUIRE(stage.is_clear());
}

//--------------------------------------------------------------------

TEST_CASE("Top-Down Matching", "[Stage]") {
  auto config = R"(
    A   >> 1
    A B >> 2
    B   >> 3
  )";
  Stage stage = create_stage(config);

  REQUIRE(apply_input(stage, "+A") == "+1");
  REQUIRE(apply_input(stage, "-A") == "-1");
  REQUIRE(stage.is_clear());

  REQUIRE(apply_input(stage, "+B") == "+3");
  REQUIRE(apply_input(stage, "-B") == "-3");
  REQUIRE(stage.is_clear());

  REQUIRE(apply_input(stage, "+A") == "+1");
  REQUIRE(apply_input(stage, "+B") == "+2");
  REQUIRE(apply_input(stage, "-B") == "-2");
  REQUIRE(apply_input(stage, "-A") == "-1");
}

//--------------------------------------------------------------------

TEST_CASE("Input is completly replaced", "[Stage]") {
  auto config = R"(
    A    >> 1
    B{C} >> 2
  )";
  Stage stage = create_stage(config);

  REQUIRE(apply_input(stage, "+A") == "+1");
  REQUIRE(apply_input(stage, "-A") == "-1");

  REQUIRE(apply_input(stage, "+B") == "");
  REQUIRE(apply_input(stage, "-B") == "+B -B");

  REQUIRE(apply_input(stage, "+B") == "");
  REQUIRE(apply_input(stage, "+C") == "+2");
  REQUIRE(apply_input(stage, "-C") == "-2");
  REQUIRE(apply_input(stage, "-B") == "");
}

//--------------------------------------------------------------------

TEST_CASE("Cursor", "[Stage]") {
  auto config = R"(
    Ext      = CapsLock
    Ext      >>
    Ext{I}   >> ArrowUp
    Ext{K}   >> ArrowDown
    Ext{J}   >> ArrowLeft
    Ext{L}   >> ArrowRight
    Ext{D}   >> Shift
    Ext{Any} >>
  )";
  Stage stage = create_stage(config);

  // I   =>
  REQUIRE(apply_input(stage, "+I") == "+I");
  REQUIRE(format_sequence(stage.sequence()) == "#I");
  REQUIRE(apply_input(stage, "-I") == "-I");
  REQUIRE(stage.is_clear());

  // CapsLock   =>
  REQUIRE(apply_input(stage, "+CapsLock") == "");
  REQUIRE(format_sequence(stage.sequence()) == "#CapsLock");
  REQUIRE(apply_input(stage, "-CapsLock") == "");
  REQUIRE(stage.is_clear());

  // CapsLock{X}  =>
  REQUIRE(apply_input(stage, "+CapsLock") == "");
  REQUIRE(apply_input(stage, "+X") == "");
  REQUIRE(apply_input(stage, "-X") == "");
  REQUIRE(apply_input(stage, "-CapsLock") == "");
  REQUIRE(stage.is_clear());

  // CapsLock{I}  => Up
  REQUIRE(apply_input(stage, "+CapsLock") == "");
  REQUIRE(format_sequence(stage.sequence()) == "#CapsLock");

  REQUIRE(apply_input(stage, "+I") == "+ArrowUp");
  REQUIRE(format_sequence(stage.sequence()) == "#CapsLock #I");
  REQUIRE(apply_input(stage, "+I") == "+ArrowUp");
  REQUIRE(format_sequence(stage.sequence()) == "#CapsLock #I");
  REQUIRE(apply_input(stage, "-I") == "-ArrowUp");
  REQUIRE(format_sequence(stage.sequence()) == "#CapsLock");

  // (CapsLock D){I}  => ShiftLeft{Up}
  REQUIRE(apply_input(stage, "+D") == "+ShiftLeft");
  REQUIRE(format_sequence(stage.sequence()) == "#CapsLock #D");

  REQUIRE(apply_input(stage, "+I") == "+ArrowUp");
  REQUIRE(format_sequence(stage.sequence()) == "#CapsLock #D #I");
  REQUIRE(apply_input(stage, "+I") == "+ArrowUp");
  REQUIRE(format_sequence(stage.sequence()) == "#CapsLock #D #I");
  REQUIRE(apply_input(stage, "-I") == "-ArrowUp");
  REQUIRE(format_sequence(stage.sequence()) == "#CapsLock #D");
  REQUIRE(apply_input(stage, "-D") == "-ShiftLeft");
  REQUIRE(format_sequence(stage.sequence()) == "#CapsLock");

  REQUIRE(apply_input(stage, "-CapsLock") == "");
  REQUIRE(stage.is_clear());
}

//--------------------------------------------------------------------

TEST_CASE("Any matches any key", "[Stage]") {
  auto config = R"(
    A{B} >> 1
    Any  >>
  )";
  Stage stage = create_stage(config);

  REQUIRE(apply_input(stage, "+A") == "");
  REQUIRE(apply_input(stage, "-A") == "");

  REQUIRE(apply_input(stage, "+B") == "");
  REQUIRE(apply_input(stage, "-B") == "");

  REQUIRE(apply_input(stage, "+A") == "");
  REQUIRE(apply_input(stage, "+B") == "+1");
  REQUIRE(apply_input(stage, "-B") == "-1");
  REQUIRE(apply_input(stage, "-A") == "");
}

//--------------------------------------------------------------------

TEST_CASE("Not in output", "[Stage]") {
  auto config = R"(
    Shift    >> Shift
    Shift{X} >> !Shift 1
  )";
  Stage stage = create_stage(config);

  // check that it temporarily released
  REQUIRE(apply_input(stage, "+ShiftLeft") == "+ShiftLeft");
  REQUIRE(format_sequence(stage.sequence()) == "#ShiftLeft");
  REQUIRE(apply_input(stage, "+X") == "-ShiftLeft +1");
  REQUIRE(format_sequence(stage.sequence()) == "#ShiftLeft #X");
  REQUIRE(apply_input(stage, "+X") == "+1");
  REQUIRE(apply_input(stage, "+X") == "+1");
  REQUIRE(apply_input(stage, "-X") == "-1");
  REQUIRE(apply_input(stage, "-ShiftLeft") == "");
  REQUIRE(stage.is_clear());

  // check that it is reapplied
  REQUIRE(apply_input(stage, "+ShiftLeft") == "+ShiftLeft");
  REQUIRE(apply_input(stage, "+X") == "-ShiftLeft +1");
  REQUIRE(apply_input(stage, "-X") == "-1");
  REQUIRE(apply_input(stage, "+X") == "+1");
  REQUIRE(apply_input(stage, "-X") == "-1");
  REQUIRE(apply_input(stage, "+Y") == "+ShiftLeft +Y");
  REQUIRE(apply_input(stage, "-Y") == "-Y");
  REQUIRE(apply_input(stage, "-ShiftLeft") == "-ShiftLeft");
}

//--------------------------------------------------------------------

TEST_CASE("Not in middle of output", "[Stage]") {
  auto config = R"(
    Shift    >> Shift
    Shift{X} >> 2 !Shift 1
  )";
  Stage stage = create_stage(config);

  // check that it temporarily released
  REQUIRE(apply_input(stage, "+ShiftLeft") == "+ShiftLeft");
  REQUIRE(format_sequence(stage.sequence()) == "#ShiftLeft");
  REQUIRE(apply_input(stage, "+X") == "+2 -ShiftLeft +1");
  REQUIRE(format_sequence(stage.sequence()) == "#ShiftLeft #X");
  REQUIRE(apply_input(stage, "+X") == "+ShiftLeft -2 +2 -ShiftLeft -1 +1");
  REQUIRE(apply_input(stage, "+X") == "+ShiftLeft -2 +2 -ShiftLeft -1 +1");
  REQUIRE(apply_input(stage, "-X") == "-1 -2");
  REQUIRE(apply_input(stage, "-ShiftLeft") == "");
  REQUIRE(stage.is_clear());

  // check that it is reapplied
  REQUIRE(apply_input(stage, "+ShiftLeft") == "+ShiftLeft");
  REQUIRE(apply_input(stage, "+X") == "+2 -ShiftLeft +1");
  REQUIRE(apply_input(stage, "-X") == "-1 -2");
  REQUIRE(apply_input(stage, "+X") == "+ShiftLeft +2 -ShiftLeft +1");
  REQUIRE(apply_input(stage, "-X") == "-1 -2");
  REQUIRE(apply_input(stage, "+Y") == "+ShiftLeft +Y");
  REQUIRE(apply_input(stage, "-Y") == "-Y");
  REQUIRE(apply_input(stage, "-ShiftLeft") == "-ShiftLeft");
}

//--------------------------------------------------------------------

TEST_CASE("Toggle Virtual", "[Stage]") {
  auto config = R"(
    ScrollLock  >> Virtual1 X Virtual2
    Virtual1{A} >> 1
    Virtual2{B} >> 2
  )";
  Stage stage = create_stage(config);

  REQUIRE(apply_input(stage, "+A") == "+A");
  REQUIRE(apply_input(stage, "-A") == "-A");
  REQUIRE(apply_input(stage, "+B") == "+B");
  REQUIRE(apply_input(stage, "-B") == "-B");

  REQUIRE(apply_input(stage, "+ScrollLock") == "+X -X");
  REQUIRE(apply_input(stage, "-ScrollLock") == "");
  REQUIRE(format_sequence(stage.sequence()) == "#Virtual1 #Virtual2");

  REQUIRE(apply_input(stage, "+A") == "+1");
  REQUIRE(apply_input(stage, "-A") == "-1");

  REQUIRE(apply_input(stage, "+B") == "+2");
  REQUIRE(apply_input(stage, "-B") == "-2");

  REQUIRE(apply_input(stage, "+ScrollLock") == "+X -X");
  REQUIRE(apply_input(stage, "-ScrollLock") == "");
  REQUIRE(stage.is_clear());

  REQUIRE(apply_input(stage, "+A") == "+A");
  REQUIRE(apply_input(stage, "-A") == "-A");
  REQUIRE(apply_input(stage, "+B") == "+B");
  REQUIRE(apply_input(stage, "-B") == "-B");
}

//--------------------------------------------------------------------

TEST_CASE("Press already pressed", "[Stage]") {
  auto config = R"(
    Shift >> Shift
    Shift{Quote} >> Shift{2}
  )";
  Stage stage = create_stage(config);

  REQUIRE(apply_input(stage, "+ShiftLeft") == "+ShiftLeft");
  REQUIRE(apply_input(stage, "+Quote") == "+2 -2");
  REQUIRE(apply_input(stage, "+Quote") == "+2 -2");
  REQUIRE(apply_input(stage, "-Quote") == "");
  REQUIRE(apply_input(stage, "+G") == "+G");
  REQUIRE(apply_input(stage, "-G") == "-G");
  REQUIRE(apply_input(stage, "-ShiftLeft") == "-ShiftLeft");
}

//--------------------------------------------------------------------

TEST_CASE("Press already pressed, with Not", "[Stage]") {
  auto config = R"(
    Shift    >> Shift
    Shift{X} >> !Shift 1
    Shift{Y} >> 1
  )";
  Stage stage = create_stage(config);

  REQUIRE(apply_input(stage, "+ShiftLeft") == "+ShiftLeft");
  REQUIRE(format_sequence(stage.sequence()) == "#ShiftLeft");
  REQUIRE(apply_input(stage, "+X") == "-ShiftLeft +1");
  REQUIRE(format_sequence(stage.sequence()) == "#ShiftLeft #X");
  REQUIRE(apply_input(stage, "+Y") == "+ShiftLeft -1 +1");
  REQUIRE(apply_input(stage, "-Y") == "");
  REQUIRE(apply_input(stage, "-X") == "-1");
  REQUIRE(apply_input(stage, "-ShiftLeft") == "-ShiftLeft");
  REQUIRE(stage.is_clear());
}
//--------------------------------------------------------------------

TEST_CASE("Press already pressed, with Not 2", "[Stage]") {
  auto config = R"(
  Shift >> Shift
  Shift{Comma}  >> !Shift IntlBackslash   # <
  Shift{Period} >> Shift{IntlBackslash}   # >
  )";
  Stage stage = create_stage(config);

  REQUIRE(apply_input(stage, "+ShiftLeft") == "+ShiftLeft");
  REQUIRE(apply_input(stage, "+Period") == "+IntlBackslash -IntlBackslash");
  REQUIRE(apply_input(stage, "-Period") == "");
  REQUIRE(apply_input(stage, "-ShiftLeft") == "-ShiftLeft");

  REQUIRE(apply_input(stage, "+ShiftLeft") == "+ShiftLeft");
  REQUIRE(apply_input(stage, "+Comma") == "-ShiftLeft +IntlBackslash");
  REQUIRE(apply_input(stage, "-Comma") == "-IntlBackslash");
  REQUIRE(apply_input(stage, "-ShiftLeft") == "");

  // Shift{< > < > < >}
  REQUIRE(apply_input(stage, "+ShiftLeft") == "+ShiftLeft");
  REQUIRE(apply_input(stage, "+Comma") == "-ShiftLeft +IntlBackslash");
  REQUIRE(apply_input(stage, "+Period") == "+ShiftLeft -IntlBackslash +IntlBackslash -IntlBackslash -ShiftLeft");
  REQUIRE(apply_input(stage, "-Comma") == "");
  REQUIRE(apply_input(stage, "-Period") == "");

  REQUIRE(apply_input(stage, "+Comma") == "+IntlBackslash");
  REQUIRE(apply_input(stage, "+Period") == "+ShiftLeft -IntlBackslash +IntlBackslash -IntlBackslash -ShiftLeft");
  REQUIRE(apply_input(stage, "-Comma") == "");
  REQUIRE(apply_input(stage, "-Period") == "");
  REQUIRE(apply_input(stage, "-ShiftLeft") == "");
}

//--------------------------------------------------------------------

TEST_CASE("Complex modifier - ordered", "[Stage]") {
  auto config = R"(
    Control >> Control
    Control{W{I}} >> A
  )";
  Stage stage = create_stage(config);

  REQUIRE(apply_input(stage, "+W") == "+W");
  REQUIRE(apply_input(stage, "-W") == "-W");

  REQUIRE(apply_input(stage, "+ControlLeft") == "+ControlLeft");
  REQUIRE(apply_input(stage, "+W") == "");
  REQUIRE(apply_input(stage, "+I") == "+A");
  REQUIRE(apply_input(stage, "-I") == "-A");
  REQUIRE(apply_input(stage, "-W") == "");

  REQUIRE(apply_input(stage, "+I") == "+I");
  REQUIRE(apply_input(stage, "-I") == "-I");
  REQUIRE(apply_input(stage, "-ControlLeft") == "-ControlLeft");

  REQUIRE(apply_input(stage, "+W") == "+W");
  REQUIRE(apply_input(stage, "+ControlLeft") == "+ControlLeft");
  REQUIRE(apply_input(stage, "+I") == "+I");
  REQUIRE(apply_input(stage, "-I") == "-I");
  REQUIRE(apply_input(stage, "-W") == "-W");
  REQUIRE(apply_input(stage, "-ControlLeft") == "-ControlLeft");
}

//--------------------------------------------------------------------

TEST_CASE("Complex modifier - unordered", "[Stage]") {
  auto config = R"(
    Shift   >> Shift
    Control >> Control
    (Control Shift){I} >> A
  )";
  Stage stage = create_stage(config);

  REQUIRE(apply_input(stage, "+ShiftLeft") == "+ShiftLeft");
  REQUIRE(apply_input(stage, "-ShiftLeft") == "-ShiftLeft");

  REQUIRE(apply_input(stage, "+ControlLeft") == "+ControlLeft");
  REQUIRE(apply_input(stage, "+ShiftLeft") == "+ShiftLeft");
  REQUIRE(format_sequence(stage.sequence()) == "#ControlLeft #ShiftLeft");
  REQUIRE(apply_input(stage, "+I") == "+A");
  REQUIRE(apply_input(stage, "-I") == "-A");
  REQUIRE(apply_input(stage, "-ShiftLeft") == "-ShiftLeft");
  REQUIRE(format_sequence(stage.sequence()) == "#ControlLeft");

  REQUIRE(apply_input(stage, "+I") == "+I");
  REQUIRE(apply_input(stage, "-I") == "-I");
  REQUIRE(apply_input(stage, "-ControlLeft") == "-ControlLeft");
  REQUIRE(stage.is_clear());

  REQUIRE(apply_input(stage, "+ShiftLeft") == "+ShiftLeft");
  REQUIRE(apply_input(stage, "+ControlLeft") == "+ControlLeft");
  REQUIRE(format_sequence(stage.sequence()) == "#ShiftLeft #ControlLeft");
  REQUIRE(apply_input(stage, "+I") == "+A");
  REQUIRE(apply_input(stage, "-I") == "-A");
  REQUIRE(apply_input(stage, "-ControlLeft") == "-ControlLeft");
  REQUIRE(apply_input(stage, "-ShiftLeft") == "-ShiftLeft");
  REQUIRE(stage.is_clear());
}

//--------------------------------------------------------------------

TEST_CASE("Might match, then no match or match", "[Stage]") {
  auto config = R"(
    D    >> 0
    A{B} >> 1
    B    >> 2
    C    >> 3
  )";
  Stage stage = create_stage(config);

  REQUIRE(apply_input(stage, "+A") == "");
  REQUIRE(apply_input(stage, "-A") == "+A -A");

  REQUIRE(apply_input(stage, "+A") == "");
  REQUIRE(apply_input(stage, "+X") == "+A +X");
  REQUIRE(apply_input(stage, "-A") == "-A");

  REQUIRE(apply_input(stage, "+A") == "");
  REQUIRE(apply_input(stage, "+D") == "+A +0");
  REQUIRE(apply_input(stage, "-D") == "-0");
  REQUIRE(apply_input(stage, "-A") == "-A");

  REQUIRE(apply_input(stage, "+A") == "");
  REQUIRE(apply_input(stage, "+C") == "+A +3");
  REQUIRE(apply_input(stage, "-C") == "-3");
  REQUIRE(apply_input(stage, "-A") == "-A");

  REQUIRE(apply_input(stage, "+A") == "");
  REQUIRE(apply_input(stage, "+B") == "+1");
  REQUIRE(apply_input(stage, "-B") == "-1");
  REQUIRE(apply_input(stage, "+B") == "+1");
  REQUIRE(apply_input(stage, "-B") == "-1");
  REQUIRE(apply_input(stage, "-A") == "");

  REQUIRE(apply_input(stage, "+B") == "+2");
  REQUIRE(apply_input(stage, "-B") == "-2");
}

//--------------------------------------------------------------------

TEST_CASE("Keyrepeat might match", "[Stage]") {
  auto config = R"(
    Space{C} >> Control{C}
  )";
  Stage stage = create_stage(config);

  REQUIRE(apply_input(stage, "+Space") == "");
  REQUIRE(apply_input(stage, "+Space") == "");
  REQUIRE(apply_input(stage, "+C") == "+ControlLeft +C -C -ControlLeft");
  REQUIRE(apply_input(stage, "+C") == "+ControlLeft +C -C -ControlLeft");
  REQUIRE(apply_input(stage, "-C") == "");
  REQUIRE(apply_input(stage, "-Space") == "");

  REQUIRE(apply_input(stage, "+Space") == "");
  REQUIRE(apply_input(stage, "+Space") == "");
  REQUIRE(apply_input(stage, "+D") == "+Space +D");
  REQUIRE(apply_input(stage, "-D") == "-D");
  REQUIRE(apply_input(stage, "-Space") == "-Space");

  REQUIRE(apply_input(stage, "+Space") == "");
  REQUIRE(apply_input(stage, "+Space") == "");
  REQUIRE(apply_input(stage, "-Space") == "+Space -Space");
}

//--------------------------------------------------------------------

TEST_CASE("Might match problem", "[Stage]") {
  auto config = R"(
    Space{C}             >> Control{C}
    IntlBackslash{Space} >> Space
  )";
  Stage stage = create_stage(config);

  REQUIRE(apply_input(stage, "+IntlBackslash") == "");
  REQUIRE(apply_input(stage, "+Space") == "+Space");
  REQUIRE(apply_input(stage, "+Space") == "");
  REQUIRE(apply_input(stage, "+Space") == "");
  REQUIRE(apply_input(stage, "-Space") == "-Space +Space -Space");
  REQUIRE(apply_input(stage, "-IntlBackslash") == "");
}

//--------------------------------------------------------------------

TEST_CASE("Any key", "[Stage]") {
  auto config = R"(
    Meta >> Meta
    Meta{Any} >> Any
    A >> B
    E >> F

    K >> Any S
  )";
  Stage stage = create_stage(config);

  REQUIRE(apply_input(stage, "+A") == "+B");
  REQUIRE(apply_input(stage, "-A") == "-B");
  REQUIRE(apply_input(stage, "+E") == "+F");
  REQUIRE(apply_input(stage, "-E") == "-F");
  REQUIRE(apply_input(stage, "+H") == "+H");
  REQUIRE(apply_input(stage, "-H") == "-H");
  REQUIRE(stage.is_clear());

  REQUIRE(apply_input(stage, "+MetaLeft") == "+MetaLeft");
  REQUIRE(apply_input(stage, "+A") == "+A");
  REQUIRE(apply_input(stage, "+E") == "+E");
  REQUIRE(apply_input(stage, "+H") == "+H");
  REQUIRE(apply_input(stage, "-A") == "-A");
  REQUIRE(apply_input(stage, "-E") == "-E");
  REQUIRE(apply_input(stage, "-H") == "-H");
  REQUIRE(apply_input(stage, "-MetaLeft") == "-MetaLeft");
  REQUIRE(stage.is_clear());

  REQUIRE(apply_input(stage, "+K") == "+S -S");
  REQUIRE(apply_input(stage, "-K") == "");
  REQUIRE(stage.is_clear());
}

//--------------------------------------------------------------------

TEST_CASE("Any key might match", "[Stage]") {
  auto config = R"(
    L Any >> E{Any}
    M Any N >> F{Any}
    O Any P >> G{Any Any}
    Q Any Any R >> H{Any Any}
  )";
  Stage stage = create_stage(config);

  REQUIRE(apply_input(stage, "+L") == "");
  REQUIRE(apply_input(stage, "+Z") == "+E +Z -Z -E");
  REQUIRE(apply_input(stage, "+Z") == "+E +Z -Z -E");
  REQUIRE(apply_input(stage, "-Z") == "");
  REQUIRE(apply_input(stage, "-L") == "");
  REQUIRE(stage.is_clear());

  REQUIRE(apply_input(stage, "+M") == "");
  REQUIRE(apply_input(stage, "+Z") == "");
  REQUIRE(apply_input(stage, "+N") == "+F +Z -Z -F");
  REQUIRE(apply_input(stage, "-N") == "");
  REQUIRE(apply_input(stage, "-Z") == "");
  REQUIRE(apply_input(stage, "-M") == "");
  REQUIRE(stage.is_clear());

  REQUIRE(apply_input(stage, "+M") == "");
  REQUIRE(apply_input(stage, "+Z") == "");
  REQUIRE(apply_input(stage, "-Z") == "");
  REQUIRE(apply_input(stage, "+N") == "+F +Z -Z -F");
  REQUIRE(apply_input(stage, "-N") == "");
  REQUIRE(apply_input(stage, "-M") == "");
  REQUIRE(stage.is_clear());

  REQUIRE(apply_input(stage, "+O") == "");
  REQUIRE(apply_input(stage, "+Z") == "");
  REQUIRE(apply_input(stage, "-Z") == "");
  REQUIRE(apply_input(stage, "+P") == "+G +Z -Z +Z -Z -G");
  REQUIRE(apply_input(stage, "-P") == "");
  REQUIRE(apply_input(stage, "-O") == "");
  REQUIRE(stage.is_clear());

  REQUIRE(apply_input(stage, "+Q") == "");
  REQUIRE(apply_input(stage, "+Z") == "");
  REQUIRE(apply_input(stage, "-Z") == "");
  REQUIRE(apply_input(stage, "+Y") == "");
  REQUIRE(apply_input(stage, "-Y") == "");
  REQUIRE(apply_input(stage, "+R") == "+H +Z +Y -Z -Y +Z +Y -Z -Y -H");
  REQUIRE(apply_input(stage, "-R") == "");
  REQUIRE(apply_input(stage, "-Q") == "");
  REQUIRE(stage.is_clear());
}

//--------------------------------------------------------------------

TEST_CASE("Output on release", "[Stage]") {
  auto config = R"(
    MetaLeft{C} >> MetaLeft{R} ^ C M
  )";
  Stage stage = create_stage(config);

  REQUIRE(apply_input(stage, "+MetaLeft") == "");
  REQUIRE(apply_input(stage, "+C") == "+MetaLeft +R -R -MetaLeft ^ +C -C +M -M");
}

//--------------------------------------------------------------------

TEST_CASE("System context", "[Stage]") {
  auto config = R"(
    A >> commandA
    B >> commandB

    [system="Linux"]
    commandA >> E

    [system="Windows"]
    commandA >> F

    [system="Windows"]
    commandB >> H

    [system="Linux"]
    commandB >> G
  )";
  Stage stage = create_stage(config);
  REQUIRE(stage.contexts().size() == 3);

#if defined(__linux__)
  REQUIRE(apply_input(stage, "+A -A") == "+E -E");
  REQUIRE(apply_input(stage, "+B -B") == "+G -G");
#elif defined(_WIN32)
  REQUIRE(apply_input(stage, "+A -A") == "+F -F");
  REQUIRE(apply_input(stage, "+B -B") == "+H -H");
#endif
}

//--------------------------------------------------------------------

TEST_CASE("System context - partially mapped", "[Stage]") {
  auto config = R"(
    # no mapping in other system
    A >> commandLinux
    B >> commandWindows
    C >> commandLinuxDefault
    D >> commandWindowsDefault

    commandLinuxDefault >> I
    commandWindowsDefault >> J

    [system="Linux"]
    commandLinux >> E
    commandLinuxDefault >> F

    [system="Windows"]
    commandWindows >> G
    commandWindowsDefault >> H
  )";
  Stage stage = create_stage(config);
  REQUIRE(stage.contexts().size() == 2);

#if defined(__linux__)
  REQUIRE(apply_input(stage, "+A -A") == "+E -E");
  REQUIRE(apply_input(stage, "+B -B") == "+B -B");
  REQUIRE(apply_input(stage, "+C -C") == "+F -F");
  REQUIRE(apply_input(stage, "+D -D") == "+J -J");
#elif defined(_WIN32)
  REQUIRE(apply_input(stage, "+A -A") == "+A -A");
  REQUIRE(apply_input(stage, "+B -B") == "+G -G");
  REQUIRE(apply_input(stage, "+C -C") == "+I -I");
  REQUIRE(apply_input(stage, "+D -D") == "+H -H");
#endif
}

//--------------------------------------------------------------------

TEST_CASE("Mapping sequence in context", "[Stage]") {
  auto config = R"(
    R >> R

    [title="Firefox"]
    A >> B
    R >> U
    X >> Y

    [title="Konsole"]
    A >> C
    R >> V
    X >> Z

    [system="Linux"]
    A >> E

    [system="Windows"]
    A >> F
  )";
  Stage stage = create_stage(config);
  REQUIRE(stage.contexts().size() == 4);
  stage.set_active_contexts({ 0, 3 }); // No program

#if defined(__linux__)
  REQUIRE(apply_input(stage, "+A -A") == "+E -E");
#elif defined(_WIN32)
  REQUIRE(apply_input(stage, "+A -A") == "+F -F");
#endif
  REQUIRE(apply_input(stage, "+R -R") == "+R -R");
  REQUIRE(apply_input(stage, "+X -X") == "+X -X"); // implicit default mapping forwards

  stage.set_active_contexts({ 0, 1, 3 }); // Firefox
  REQUIRE(apply_input(stage, "+A -A") == "+B -B");
  REQUIRE(apply_input(stage, "+R -R") == "+R -R");
  REQUIRE(apply_input(stage, "+X -X") == "+Y -Y");

  stage.set_active_contexts({ 0, 2, 3 }); // Konsole
  REQUIRE(apply_input(stage, "+A -A") == "+C -C");
  REQUIRE(apply_input(stage, "+R -R") == "+R -R");
  REQUIRE(apply_input(stage, "+X -X") == "+Z -Z");
}

//--------------------------------------------------------------------

TEST_CASE("Mapping sequence in context - comparison", "[Stage]") {
  auto config = R"(
    A >> command
    R >> command2
    command2 >> R
    X >> command3

    [system="Linux"]
    command >> E

    [system="Windows"]
    command >> F

    [title="Firefox"]
    command >> B
    command2 >> U
    command3 >> Y

    [title="Konsole"]
    command >> C
    command2 >> V
    command3 >> Z
  )";
  Stage stage = create_stage(config);
  REQUIRE(stage.contexts().size() == 4);
  stage.set_active_contexts({ 0, 1 }); // No program

#if defined(__linux__)
  REQUIRE(apply_input(stage, "+A -A") == "+E -E");
#elif defined(_WIN32)
  REQUIRE(apply_input(stage, "+A -A") == "+F -F");
#endif
  REQUIRE(apply_input(stage, "+R -R") == "+R -R");
  REQUIRE(apply_input(stage, "+X -X") == "+X -X"); // no default mapping for command3

  stage.set_active_contexts({ 0, 1, 2 }); // Firefox
  REQUIRE(apply_input(stage, "+A -A") == "+B -B");
  REQUIRE(apply_input(stage, "+R -R") == "+U -U");
  REQUIRE(apply_input(stage, "+X -X") == "+Y -Y");

  stage.set_active_contexts({ 0, 1, 3 }); // Konsole
  REQUIRE(apply_input(stage, "+A -A") == "+C -C");
  REQUIRE(apply_input(stage, "+R -R") == "+V -V");
  REQUIRE(apply_input(stage, "+X -X") == "+Z -Z");
}

//--------------------------------------------------------------------

TEST_CASE("Restore default context", "[Stage]") {
  auto config = R"(
    [title="AnyDesk"]
    Any >> Any

    [default]
    R >> R

    [title="Firefox"]
    A >> B
    R >> U
    X >> Y

    [title="Konsole"]
    A >> C
    R >> V
    X >> Z

    [system="Linux"]
    A >> E

    [system="Windows"]
    A >> F
  )";
  Stage stage = create_stage(config);
  REQUIRE(stage.contexts().size() == 5);
  stage.set_active_contexts({ 1, 4 }); // No program

#if defined(__linux__)
  REQUIRE(apply_input(stage, "+A -A") == "+E -E");
#elif defined(_WIN32)
  REQUIRE(apply_input(stage, "+A -A") == "+F -F");
#endif
  REQUIRE(apply_input(stage, "+R -R") == "+R -R");
  REQUIRE(apply_input(stage, "+X -X") == "+X -X"); // implicit default mapping forwards

  stage.set_active_contexts({ 1, 2, 4 }); // Firefox
  REQUIRE(apply_input(stage, "+A -A") == "+B -B");
  REQUIRE(apply_input(stage, "+R -R") == "+R -R");
  REQUIRE(apply_input(stage, "+X -X") == "+Y -Y");

  stage.set_active_contexts({ 1, 3, 4 }); // Konsole
  REQUIRE(apply_input(stage, "+A -A") == "+C -C");
  REQUIRE(apply_input(stage, "+R -R") == "+R -R");
  REQUIRE(apply_input(stage, "+X -X") == "+Z -Z");

  stage.set_active_contexts({ 0, 0, 1, 4 }); // AnyDesk
  REQUIRE(apply_input(stage, "+A -A") == "+A -A");
  REQUIRE(apply_input(stage, "+R -R") == "+R -R");
  REQUIRE(apply_input(stage, "+X -X") == "+X -X");
}

//--------------------------------------------------------------------

TEST_CASE("Trigger action", "[Stage]") {
  auto config = R"(
    A >> A $(system command 1)
    B >> $(system command 2) B
    C >> E{F} $(system (command) 3) G{H}
    D >> ^ $(system command 4)
    E >> $(system command 5) ^
  )";
  Stage stage = create_stage(config);

  CHECK(apply_input(stage, "+A") == "+A -A +Action0");
  CHECK(apply_input(stage, "-A") == "-Action0");
  CHECK(apply_input(stage, "+B") == "+Action1 +B");
  CHECK(apply_input(stage, "-B") == "-B -Action1");
  CHECK(apply_input(stage, "+C") == "+E +F -F -E +Action2 +G +H -H -G");
  CHECK(apply_input(stage, "-C") == "-Action2");
  CHECK(apply_input(stage, "+D") == "^ +Action3");
  CHECK(apply_input(stage, "-D") == "-Action3");
  CHECK(apply_input(stage, "+E") == "+Action4 ^");
  CHECK(apply_input(stage, "-E") == "-Action4");
}

//--------------------------------------------------------------------

TEST_CASE("Release output with modifiers before next output", "[Stage]") {
  auto config = R"(
    A >> ShiftLeft{B}
    S >> T
  )";
  Stage stage = create_stage(config);

  REQUIRE(apply_input(stage, "+A") == "+ShiftLeft +B -B -ShiftLeft");
  REQUIRE(apply_input(stage, "+A") == "+ShiftLeft +B -B -ShiftLeft");
  REQUIRE(apply_input(stage, "-A") == "");

  // completely release output with modifiers before next output
  REQUIRE(apply_input(stage, "+A") == "+ShiftLeft +B -B -ShiftLeft");
  REQUIRE(apply_input(stage, "+C") == "+C");
  REQUIRE(apply_input(stage, "+C") == "+C");
  REQUIRE(apply_input(stage, "-C") == "-C");

  // but do not for outputs without modifiers
  REQUIRE(apply_input(stage, "+S") == "+T");
  REQUIRE(apply_input(stage, "+S") == "+T");
  REQUIRE(apply_input(stage, "+C") == "+C");
  REQUIRE(apply_input(stage, "+C") == "+C");
  REQUIRE(apply_input(stage, "-C") == "-C");
  REQUIRE(apply_input(stage, "-S") == "-T");

  // do not press/release already pressed
  REQUIRE(apply_input(stage, "+ShiftLeft") == "+ShiftLeft");
  REQUIRE(apply_input(stage, "+A") == "+B -B");
  REQUIRE(apply_input(stage, "+A") == "+B -B");
  REQUIRE(apply_input(stage, "+C") == "+C");
  REQUIRE(apply_input(stage, "+C") == "+C");
  REQUIRE(apply_input(stage, "-A") == "");
  REQUIRE(apply_input(stage, "-C") == "-C");
  REQUIRE(apply_input(stage, "-ShiftLeft") == "-ShiftLeft");
}

//--------------------------------------------------------------------

TEST_CASE("Old common modifier behaviour", "[Stage]") {
  auto config = R"(
    Alt = AltLeft

    # used to be implicitly mapped
    Shift   >> Shift
    Control >> Control
    Alt     >> Alt

    Alt{W}  >> !Alt IntlBackslash
    Alt{X}  >> !Alt Shift{IntlBackslash}
  )";
  Stage stage = create_stage(config);

  REQUIRE(apply_input(stage, "+AltLeft") == "+AltLeft");
  REQUIRE(apply_input(stage, "+W") == "-AltLeft +IntlBackslash");
  REQUIRE(apply_input(stage, "-W") == "-IntlBackslash");
  REQUIRE(apply_input(stage, "-AltLeft") == "");

  REQUIRE(apply_input(stage, "+AltLeft") == "+AltLeft");
  REQUIRE(apply_input(stage, "+X") == "-AltLeft +ShiftLeft +IntlBackslash -IntlBackslash -ShiftLeft");
  REQUIRE(apply_input(stage, "-X") == "");
  REQUIRE(apply_input(stage, "-AltLeft") == "");
}

//--------------------------------------------------------------------

TEST_CASE("New common modifier behaviour", "[Stage]") {
  auto config = R"(
    Alt = AltLeft
    Shift         >> Shift
    Control       >> Control
    (Control Alt) >> !Control Alt   # Alt mouse modifier

    Alt{W} >> IntlBackslash
    Alt{X} >> Shift{IntlBackslash}
  )";
  Stage stage = create_stage(config);

  REQUIRE(apply_input(stage, "+AltLeft") == "");
  REQUIRE(apply_input(stage, "+W") == "+IntlBackslash");
  REQUIRE(apply_input(stage, "-W") == "-IntlBackslash");
  REQUIRE(apply_input(stage, "-AltLeft") == "");

  REQUIRE(apply_input(stage, "+AltLeft") == "");
  REQUIRE(apply_input(stage, "+X") == "+ShiftLeft +IntlBackslash -IntlBackslash -ShiftLeft");
  REQUIRE(apply_input(stage, "-X") == "");
  REQUIRE(apply_input(stage, "-AltLeft") == "");

  REQUIRE(apply_input(stage, "+AltLeft") == "");
  REQUIRE(apply_input(stage, "+F") == "+AltLeft +F");
  REQUIRE(apply_input(stage, "-F") == "-F");
  REQUIRE(apply_input(stage, "-AltLeft") == "-AltLeft");
  REQUIRE(stage.is_clear());

  REQUIRE(apply_input(stage, "+AltLeft") == "");
  REQUIRE(apply_input(stage, "+ControlLeft") == "+AltLeft");
  REQUIRE(apply_input(stage, "-ControlLeft") == "-AltLeft");
  REQUIRE(apply_input(stage, "-AltLeft") == "");
  REQUIRE(stage.is_clear());

  REQUIRE(apply_input(stage, "+ControlLeft") == "+ControlLeft");
  REQUIRE(apply_input(stage, "+AltLeft") == "-ControlLeft +AltLeft");
  REQUIRE(apply_input(stage, "-ControlLeft") == "");
  REQUIRE(apply_input(stage, "-AltLeft") == "-AltLeft");
}

//--------------------------------------------------------------------

TEST_CASE("Continue matching after failed might-match", "[Stage]") {
  auto config = R"(
    A{B{C}}   >> Z
    A{B}      >> Y
    A{D{E}}   >> W
    A         >> X
    D{F}      >> U
  )";
  Stage stage = create_stage(config);

  REQUIRE(apply_input(stage, "+A") == "");
  REQUIRE(apply_input(stage, "-A") == "+X -X");

  REQUIRE(apply_input(stage, "+A") == "");
  REQUIRE(apply_input(stage, "+B") == "");
  REQUIRE(apply_input(stage, "-B") == "+Y -Y");
  REQUIRE(apply_input(stage, "-A") == "");

  REQUIRE(apply_input(stage, "+A") == "");
  REQUIRE(apply_input(stage, "+B") == "");
  REQUIRE(apply_input(stage, "+C") == "+Z");
  REQUIRE(apply_input(stage, "-C") == "-Z");
  REQUIRE(apply_input(stage, "-B") == "");
  REQUIRE(apply_input(stage, "-A") == "");

  // when might-match fails, look for longest exact match
  REQUIRE(apply_input(stage, "+A") == "");
  REQUIRE(apply_input(stage, "+S") == "+X +S");
  REQUIRE(apply_input(stage, "-S") == "-S -X");
  REQUIRE(apply_input(stage, "-A") == "");

  REQUIRE(apply_input(stage, "+A") == "");
  REQUIRE(apply_input(stage, "+B") == "");
  REQUIRE(apply_input(stage, "+S") == "+Y +S");
  REQUIRE(apply_input(stage, "-S") == "-S -Y");
  REQUIRE(apply_input(stage, "-B") == "");
  REQUIRE(apply_input(stage, "-A") == "");

  REQUIRE(apply_input(stage, "+A") == "");
  REQUIRE(apply_input(stage, "+D") == "");
  REQUIRE(apply_input(stage, "+S") == "+X +D +S");
  REQUIRE(apply_input(stage, "-S") == "-S -X");
  REQUIRE(apply_input(stage, "-D") == "-D");
  REQUIRE(apply_input(stage, "-A") == "");

  REQUIRE(apply_input(stage, "+A") == "");
  REQUIRE(apply_input(stage, "+D") == "");
  REQUIRE(apply_input(stage, "+F") == "+X +U");
  REQUIRE(apply_input(stage, "-F") == "-U -X");
  REQUIRE(apply_input(stage, "-D") == "");
  REQUIRE(apply_input(stage, "-A") == "");
}

//--------------------------------------------------------------------

TEST_CASE("Timeout Hold", "[Stage]") {
  auto config = R"(
    A{1000ms} >> X
  )";
  Stage stage = create_stage(config);

  CHECK(apply_input(stage, "+A") == "1000ms");
  CHECK(apply_input(stage, make_timeout_ms(271)) == "+A");
  CHECK(apply_input(stage, "+B") == "+B");
  CHECK(apply_input(stage, "-B") == "-B");
  CHECK(apply_input(stage, "-A") == "-A");
  REQUIRE(stage.is_clear());

  CHECK(apply_input(stage, "+A") == "1000ms");
  CHECK(apply_input(stage, make_timeout_ms(1000)) == "+X");
  CHECK(apply_input(stage, "+A") == "1000ms");
  CHECK(apply_input(stage, make_timeout_ms(1000)) == "+X");
  CHECK(apply_input(stage, "+A") == "1000ms");
  // release while waiting for timeout - send elapsed time
  // output is suppressed when timeout matched once
  CHECK(apply_input(stage, make_timeout_ms(271)) == "");
  CHECK(apply_input(stage, "-A") == "-X");
  REQUIRE(stage.is_clear());

  CHECK(apply_input(stage, "+ShiftLeft") == "+ShiftLeft");
  CHECK(apply_input(stage, "+A") == "1000ms");
  // release while waiting for timeout - send elapsed time
  CHECK(apply_input(stage, make_timeout_ms(271)) == "+A");
  CHECK(apply_input(stage, "-A") == "-A");
  CHECK(apply_input(stage, "-ShiftLeft") == "-ShiftLeft");
  REQUIRE(stage.is_clear());

  CHECK(apply_input(stage, "+ShiftLeft") == "+ShiftLeft");
  CHECK(apply_input(stage, "+A") == "1000ms");
  // another event while waiting for timeout - send elapsed time
  CHECK(apply_input(stage, make_timeout_ms(271)) == "+A");
  CHECK(apply_input(stage, "-ShiftLeft") == "-ShiftLeft");
  CHECK(apply_input(stage, "-A") == "-A");
  REQUIRE(stage.is_clear());
}

//--------------------------------------------------------------------

TEST_CASE("Timeout Hold, Another mapping", "[Stage]") {
  auto config = R"(
    A{1000ms} >> X
    A >> Z
  )";
  Stage stage = create_stage(config);

  // key press which cancels timeout is currently the trigger
  CHECK(apply_input(stage, "+A") == "1000ms");
  CHECK(apply_input(stage, make_timeout_ms(271)) == "+Z");
  CHECK(apply_input(stage, "+B") == "+B");
  CHECK(apply_input(stage, "-B") == "-B");
  CHECK(apply_input(stage, "-A") == "-Z");
  REQUIRE(stage.is_clear());

  CHECK(apply_input(stage, "+A") == "1000ms");
  CHECK(apply_input(stage, make_timeout_ms(1000)) == "+X");
  CHECK(apply_input(stage, "+A") == "1000ms");
  CHECK(apply_input(stage, make_timeout_ms(1000)) == "+X");
  CHECK(apply_input(stage, "+A") == "1000ms");
  // release while waiting for timeout - send elapsed time
  // output is suppressed when timeout matched once
  CHECK(apply_input(stage, make_timeout_ms(271)) == "");
  CHECK(apply_input(stage, "-A") == "-X");
  REQUIRE(stage.is_clear());

  CHECK(apply_input(stage, "+A") == "1000ms");
  CHECK(apply_input(stage, make_timeout_ms(271)) == "+Z");
  CHECK(apply_input(stage, "-A") == "-Z");
  REQUIRE(stage.is_clear());
}

//--------------------------------------------------------------------

TEST_CASE("Timeout", "[Stage]") {
  auto config = R"(
    A 1000ms >> X
  )";
  Stage stage = create_stage(config);

  CHECK(apply_input(stage, "+A") == "");
  CHECK(apply_input(stage, "+A") == "");
  CHECK(apply_input(stage, "+A") == "");
  CHECK(apply_input(stage, "-A") == "1000ms");
  CHECK(apply_input(stage, "+B") == "+A -A +B");
  CHECK(apply_input(stage, "-B") == "-B");
  REQUIRE(stage.is_clear());

  CHECK(apply_input(stage, "+A") == "");
  CHECK(apply_input(stage, "+A") == "");
  CHECK(apply_input(stage, "+A") == "");
  CHECK(apply_input(stage, "-A") == "1000ms");
  CHECK(apply_input(stage, make_timeout_ms(1000)) == "+X -X");
  REQUIRE(stage.is_clear());
}

//--------------------------------------------------------------------

TEST_CASE("Timeout Switch", "[Stage]") {
  auto config = R"(
    A{2000ms} >> X
    A{1000ms} >> Y
    A{500ms}  >> Z
  )";
  Stage stage = create_stage(config);

  CHECK(apply_input(stage, "+A") == "2000ms");
  CHECK(apply_input(stage, make_timeout_ms(2000)) == "+X");
  CHECK(apply_input(stage, "+A") == "2000ms");
  CHECK(apply_input(stage, make_timeout_ms(2000)) == "+X");
  CHECK(apply_input(stage, "+A") == "2000ms");
  CHECK(apply_input(stage, make_timeout_ms(1999)) == "");
  CHECK(apply_input(stage, "-A") == "-X");
  REQUIRE(stage.is_clear());

  CHECK(apply_input(stage, "+A") == "2000ms");
  CHECK(apply_input(stage, make_timeout_ms(1999)) == "+Y");
  CHECK(apply_input(stage, "-A") == "-Y");
  REQUIRE(stage.is_clear());

  CHECK(apply_input(stage, "+A") == "2000ms");
  CHECK(apply_input(stage, make_timeout_ms(1000)) == "+Y");
  CHECK(apply_input(stage, "-A") == "-Y");
  REQUIRE(stage.is_clear());

  CHECK(apply_input(stage, "+A") == "2000ms");
  CHECK(apply_input(stage, make_timeout_ms(999)) == "+Z");
  CHECK(apply_input(stage, "-A") == "-Z");
  REQUIRE(stage.is_clear());

  CHECK(apply_input(stage, "+A") == "2000ms");
  CHECK(apply_input(stage, make_timeout_ms(500)) == "+Z");
  CHECK(apply_input(stage, "-A") == "-Z");
  REQUIRE(stage.is_clear());

  CHECK(apply_input(stage, "+A") == "2000ms");
  CHECK(apply_input(stage, make_timeout_ms(499)) == "+A");
  CHECK(apply_input(stage, "-A") == "-A");
  REQUIRE(stage.is_clear());
}

//--------------------------------------------------------------------

TEST_CASE("Timeout Sequence #1", "[Stage]") {
  auto config = R"(
    A{500ms} B >> X
  )";
  Stage stage = create_stage(config);

  // key repeat is ignored
  CHECK(apply_input(stage, "+A") == "500ms");
  CHECK(apply_input(stage, "+A") == "");
  CHECK(apply_input(stage, make_timeout_ms(500)) == "");
  CHECK(apply_input(stage, "+A") == "");
  CHECK(apply_input(stage, "-A") == "");
  CHECK(apply_input(stage, "+B") == "+X");
  CHECK(apply_input(stage, "+B") == "+B");
  CHECK(apply_input(stage, "+B") == "+B");
  CHECK(apply_input(stage, "-B") == "-B -X");
  REQUIRE(stage.is_clear());

  CHECK(apply_input(stage, "+A") == "500ms");
  CHECK(apply_input(stage, "+A") == "");
  CHECK(apply_input(stage, make_timeout_ms(300)) == "+A");
  CHECK(apply_input(stage, "-A") == "-A");
  CHECK(apply_input(stage, "+C") == "+C");
  CHECK(apply_input(stage, "+C") == "+C");
  CHECK(apply_input(stage, "-C") == "-C");
  REQUIRE(stage.is_clear());

  CHECK(apply_input(stage, "+A") == "500ms");
  CHECK(apply_input(stage, make_timeout_ms(500)) == "");
  CHECK(apply_input(stage, "-A") == "");
  CHECK(apply_input(stage, "+C") == "+A -A +C");
  CHECK(apply_input(stage, "+C") == "+C");
  CHECK(apply_input(stage, "-C") == "-C");
  REQUIRE(stage.is_clear());
}

//--------------------------------------------------------------------

TEST_CASE("Timeout Sequence #2", "[Stage]") {
  auto config = R"(
    A 500ms B >> X
  )";
  Stage stage = create_stage(config);

  // key repeat is ignored
  CHECK(apply_input(stage, "+A") == "");
  CHECK(apply_input(stage, "+A") == "");
  CHECK(apply_input(stage, "-A") == "500ms");
  CHECK(apply_input(stage, make_timeout_ms(500)) == "");
  CHECK(apply_input(stage, "+B") == "+X");
  CHECK(apply_input(stage, "+B") == "+B");
  CHECK(apply_input(stage, "+B") == "+B");
  CHECK(apply_input(stage, "-B") == "-B -X");
  REQUIRE(stage.is_clear());

  CHECK(apply_input(stage, "+A") == "");
  CHECK(apply_input(stage, "-A") == "500ms");
  CHECK(apply_input(stage, "+C") == "+A -A +C");
  CHECK(apply_input(stage, "+C") == "+C");
  CHECK(apply_input(stage, "-C") == "-C");
  REQUIRE(stage.is_clear());

  CHECK(apply_input(stage, "+A") == "");
  CHECK(apply_input(stage, "-A") == "500ms");
  CHECK(apply_input(stage, make_timeout_ms(500)) == "");
  CHECK(apply_input(stage, "+C") == "+A -A +C");
  CHECK(apply_input(stage, "+C") == "+C");
  CHECK(apply_input(stage, "-C") == "-C");
  REQUIRE(stage.is_clear());
}

//--------------------------------------------------------------------

TEST_CASE("Not Timeout Hold", "[Stage]") {
  auto config = R"(
    A{!1000ms} >> X
  )";
  Stage stage = create_stage(config);

  CHECK(apply_input(stage, "+A") == "1000ms");
  CHECK(apply_input(stage, make_timeout_ms(999)) == "");
  CHECK(apply_input(stage, "-A") == "+X -X");
  REQUIRE(stage.is_clear());
  
  CHECK(apply_input(stage, "+A") == "1000ms");
  CHECK(apply_input(stage, make_timeout_ms(1000)) == "+A");
  CHECK(apply_input(stage, "+A") == "1000ms");
  CHECK(apply_input(stage, make_timeout_ms(1000)) == "+A");
  CHECK(apply_input(stage, "+A") == "1000ms");
  // output is suppressed when timeout was exceeded once
  CHECK(apply_input(stage, make_timeout_ms(271)) == "");
  CHECK(apply_input(stage, "-A") == "-A +A -A");
  REQUIRE(stage.is_clear());

  CHECK(apply_input(stage, "+A") == "1000ms");
  CHECK(apply_input(stage, make_timeout_ms(271)) == "");
  CHECK(apply_input(stage, "+B") == "+A +B");
  CHECK(apply_input(stage, "+B") == "+B");
  CHECK(apply_input(stage, "-B") == "-B");
  CHECK(apply_input(stage, "-A") == "-A");
  REQUIRE(stage.is_clear());
}

//--------------------------------------------------------------------

TEST_CASE("Not Timeout", "[Stage]") {
  auto config = R"(
    A !1000ms >> X
  )";
  Stage stage = create_stage(config);

  CHECK(apply_input(stage, "+A") == "");
  CHECK(apply_input(stage, "-A") == "1000ms");
  CHECK(apply_input(stage, make_timeout_ms(999)) == "+X -X");
  REQUIRE(stage.is_clear());

  CHECK(apply_input(stage, "+A") == "");
  CHECK(apply_input(stage, "-A") == "1000ms");
  CHECK(apply_input(stage, make_timeout_ms(1000)) == "+A -A");
  REQUIRE(stage.is_clear());
}

//--------------------------------------------------------------------

TEST_CASE("Timeout Xcape", "[Stage]") {
  auto config = R"(
    ControlLeft{!500ms} >> Escape
    ButtonLeft >> ButtonLeft
  )";
  Stage stage = create_stage(config);

  // long press - output Control, do not output Escape
  CHECK(apply_input(stage, "+ControlLeft") == "500ms");
  CHECK(apply_input(stage, make_timeout_ms(500)) == "+ControlLeft");
  CHECK(apply_input(stage, "+ControlLeft") == "500ms");
  CHECK(apply_input(stage, make_timeout_ms(500)) == "+ControlLeft");
  CHECK(apply_input(stage, "+ControlLeft") == "500ms");
  // release while waiting for timeout - send elapsed time
  CHECK(apply_input(stage, make_timeout_ms(271)) == "");
  // output is suppressed when timeout was exceeded once
  CHECK(apply_input(stage, "-ControlLeft") == "-ControlLeft +ControlLeft -ControlLeft");
  REQUIRE(stage.is_clear());

  // short press - output Escape
  CHECK(apply_input(stage, "+ControlLeft") == "500ms");
  CHECK(apply_input(stage, make_timeout_ms(271)) == "");
  CHECK(apply_input(stage, "-ControlLeft") == "+Escape -Escape");
  REQUIRE(stage.is_clear());
  
  // press with another key fast - output Control
  CHECK(apply_input(stage, "+ControlLeft") == "500ms");
  CHECK(apply_input(stage, make_timeout_ms(271)) == "");
  CHECK(apply_input(stage, "+C") == "+ControlLeft +C");
  CHECK(apply_input(stage, "-C") == "-C");
  CHECK(apply_input(stage, "-ControlLeft") == "-ControlLeft");
  REQUIRE(stage.is_clear());

  // press with another key slow - output Control
  CHECK(apply_input(stage, "+ControlLeft") == "500ms");
  CHECK(apply_input(stage, make_timeout_ms(500)) == "+ControlLeft");
  CHECK(apply_input(stage, "+ControlLeft") == "500ms");
  CHECK(apply_input(stage, make_timeout_ms(271)) == "");
  CHECK(apply_input(stage, "+C") == "+ControlLeft +C");
  CHECK(apply_input(stage, "+C") == "+C");
  CHECK(apply_input(stage, "-C") == "-C");
  CHECK(apply_input(stage, "-ControlLeft") == "-ControlLeft");
  REQUIRE(stage.is_clear());

  // press with mouse button slow - output Control
  CHECK(apply_input(stage, "+ControlLeft") == "500ms");
  CHECK(apply_input(stage, make_timeout_ms(500)) == "+ControlLeft");
  CHECK(apply_input(stage, "+ControlLeft") == "500ms");
  CHECK(apply_input(stage, make_timeout_ms(271)) == "");
  CHECK(apply_input(stage, "+ButtonLeft") == "+ControlLeft +ButtonLeft");
  CHECK(apply_input(stage, "-ButtonLeft") == "-ButtonLeft");
  // key repeat on keyboard
  CHECK(apply_input(stage, "+ControlLeft") == "500ms");
  CHECK(apply_input(stage, make_timeout_ms(271)) == "");
  CHECK(apply_input(stage, "-ControlLeft") == "-ControlLeft +ControlLeft -ControlLeft");
  REQUIRE(stage.is_clear());

  // press with mouse button fast - output Control
  CHECK(apply_input(stage, "+ControlLeft") == "500ms");
  CHECK(apply_input(stage, make_timeout_ms(271)) == "");
  CHECK(apply_input(stage, "+ButtonLeft") == "+ControlLeft +ButtonLeft");
  CHECK(apply_input(stage, "-ButtonLeft") == "-ButtonLeft");
  // key repeat on keyboard
  CHECK(apply_input(stage, "+ControlLeft") == "500ms");
  CHECK(apply_input(stage, make_timeout_ms(271)) == "");
  CHECK(apply_input(stage, "-ControlLeft") == "-ControlLeft +ControlLeft -ControlLeft");
  REQUIRE(stage.is_clear());

  // press with mouse button fast - output Control
  CHECK(apply_input(stage, "+ControlLeft") == "500ms");
  CHECK(apply_input(stage, make_timeout_ms(271)) == "");
  CHECK(apply_input(stage, "+ButtonLeft") == "+ControlLeft +ButtonLeft");
  CHECK(apply_input(stage, "-ButtonLeft") == "-ButtonLeft");
  CHECK(apply_input(stage, "-ControlLeft") == "-ControlLeft");
  REQUIRE(stage.is_clear());
}

//--------------------------------------------------------------------

TEST_CASE("Timeout MinMax", "[Stage]") {
  auto config = R"(
    A{500ms !500ms} >> B
  )";
  Stage stage = create_stage(config);

  // too short
  CHECK(apply_input(stage, "+A") == "500ms");
  CHECK(apply_input(stage, make_timeout_ms(499)) == "+A");
  CHECK(apply_input(stage, "-A") == "-A");

  // in time
  CHECK(apply_input(stage, "+A") == "500ms");
  CHECK(apply_input(stage, make_timeout_ms(500)) == "500ms");
  CHECK(apply_input(stage, make_timeout_ms(499)) == "");
  CHECK(apply_input(stage, "-A") == "+B -B");

  // too long
  CHECK(apply_input(stage, "+A") == "500ms");
  CHECK(apply_input(stage, make_timeout_ms(500)) == "500ms");
  CHECK(apply_input(stage, make_timeout_ms(500)) == "+A 500ms");
  CHECK(apply_input(stage, make_timeout_ms(271)) == "");
  // output is suppressed when timeout was exceeded once
  CHECK(apply_input(stage, "-A") == "-A");
}

//--------------------------------------------------------------------

TEST_CASE("Timeout Morse", "[Stage]") {
  auto config = R"(
    e = 500ms       # end
    c = !500ms      # continue
    s = X{!200ms}   # short
    l = X{200ms}    # long

    s c l e         >> A
    l c s c s c s e >> B
    s c s c s e     >> C
    l c s c s e     >> D
    s e             >> E

    X >> Y
  )";
  Stage stage = create_stage(config);

  // A
  CHECK(apply_input(stage, "+X") == "200ms");            // waiting for short
  CHECK(apply_input(stage, make_timeout_ms(100)) == ""); // waiting for release
  CHECK(apply_input(stage, "-X") == "500ms");            // waiting for continue
  CHECK(apply_input(stage, make_timeout_ms(123)) == ""); // waiting for press
  CHECK(apply_input(stage, "+X") == "200ms");            // waiting for long
  CHECK(apply_input(stage, make_timeout_ms(200)) == ""); // waiting for release
  CHECK(apply_input(stage, "+X") == "");                 // key repeat ignored
  CHECK(apply_input(stage, "+X") == "");                 // key repeat ignored
  CHECK(apply_input(stage, "-X") == "500ms");            // waiting for end
  CHECK(apply_input(stage, make_timeout_ms(500)) == "+A -A");
  REQUIRE(stage.is_clear());

  // ERROR
  CHECK(apply_input(stage, "+X") == "200ms");            
  CHECK(apply_input(stage, make_timeout_ms(200)) == ""); // long
  CHECK(apply_input(stage, "+X") == "");                 // key repeat ignored
  CHECK(apply_input(stage, "+X") == "");                 // key repeat ignored
  CHECK(apply_input(stage, "-X") == "500ms");
  CHECK(apply_input(stage, make_timeout_ms(500)) == "+Y -Y");
  REQUIRE(stage.is_clear());

  // E
  CHECK(apply_input(stage, "+X") == "200ms");            // waiting for short
  CHECK(apply_input(stage, make_timeout_ms(100)) == "");
  CHECK(apply_input(stage, "-X") == "500ms");            // waiting for end
  CHECK(apply_input(stage, make_timeout_ms(500)) == "+E -E");
  REQUIRE(stage.is_clear());

  // ERROR
  CHECK(apply_input(stage, "+X") == "200ms");
  CHECK(apply_input(stage, make_timeout_ms(200)) == ""); // long
  CHECK(apply_input(stage, "+X") == "");                 // key repeat ignored
  CHECK(apply_input(stage, "+X") == "");                 // key repeat ignored
  CHECK(apply_input(stage, "-X") == "500ms");
  CHECK(apply_input(stage, make_timeout_ms(123)) == "");
  CHECK(apply_input(stage, "+X") == "200ms");
  CHECK(apply_input(stage, make_timeout_ms(200)) == "+Y -Y"); // long (restart)
  CHECK(apply_input(stage, "+X") == "");                 // key repeat ignored
  CHECK(apply_input(stage, "+X") == "");                 // key repeat ignored
  CHECK(apply_input(stage, "-X") == "500ms");
  CHECK(apply_input(stage, make_timeout_ms(500)) == "+Y -Y");
  REQUIRE(stage.is_clear());

  // D
  CHECK(apply_input(stage, "+X") == "200ms");            // waiting for long
  CHECK(apply_input(stage, make_timeout_ms(200)) == ""); // waiting for release
  CHECK(apply_input(stage, "+X") == "");                 // key repeat ignored
  CHECK(apply_input(stage, "+X") == "");                 // key repeat ignored
  CHECK(apply_input(stage, "-X") == "500ms");            // waiting for continue
  CHECK(apply_input(stage, make_timeout_ms(123)) == ""); // waiting for press
  CHECK(apply_input(stage, "+X") == "200ms");            // waiting for short
  CHECK(apply_input(stage, make_timeout_ms(100)) == ""); // waiting for release
  CHECK(apply_input(stage, "-X") == "500ms");            // waiting for continue
  CHECK(apply_input(stage, make_timeout_ms(123)) == ""); // waiting for press
  CHECK(apply_input(stage, "+X") == "200ms");            // waiting for short
  CHECK(apply_input(stage, make_timeout_ms(100)) == ""); // waiting for release
  CHECK(apply_input(stage, "-X") == "500ms");            // waiting for end
  CHECK(apply_input(stage, make_timeout_ms(500)) == "+D -D");
  REQUIRE(stage.is_clear());
}
