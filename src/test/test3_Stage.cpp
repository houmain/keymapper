
#include "test.h"
#include "config/ParseConfig.h"
#include "runtime/Stage.h"

namespace {
  Stage create_stage(const char* string) {
    static auto parse_config = ParseConfig();
    auto stream = std::stringstream(string);
    auto config = parse_config(stream);
    auto mappings = std::vector<Mapping>();
    for (auto& command : config.commands)
      mappings.push_back({
        std::move(command.input),
        std::move(command.default_mapping)
      });
    return Stage(std::move(mappings), { });
  }

  template<size_t N>
  std::string apply_input(Stage& stage, const char(&input)[N]) {
    // apply_input all input events and concatenate output
    auto sequence = KeySequence();
    for (auto event : parse_sequence(input))
      for (auto output : stage.apply_input(event))
        sequence.push_back(output);
    return format_sequence(sequence);
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
  REQUIRE(format_sequence(stage.sequence()) == "");

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
  REQUIRE(format_sequence(stage.sequence()) == "");

  // +M S  =>  B
  REQUIRE(apply_input(stage, "+M") == "");
  REQUIRE(apply_input(stage, "+S") == "+B");
  REQUIRE(apply_input(stage, "-S") == "-B");
  REQUIRE(apply_input(stage, "-M") == "");
  REQUIRE(format_sequence(stage.sequence()) == "");

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
  REQUIRE(format_sequence(stage.sequence()) == "");
}

//--------------------------------------------------------------------

TEST_CASE("Combo", "[Stage]") {
  auto config = R"(
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
    A >> A
    Any >>
  )";
  Stage stage = create_stage(config);

  // A => A
  REQUIRE(apply_input(stage, "+A") == "+A");
  REQUIRE(format_sequence(stage.sequence()) == "#A");
  REQUIRE(apply_input(stage, "-A") == "-A");
  REQUIRE(format_sequence(stage.sequence()) == "");

  // B C ShiftLeft{D} => ShiftLeft
  REQUIRE(apply_input(stage, "+B -B") == "");
  REQUIRE(format_sequence(stage.sequence()) == "");
  REQUIRE(apply_input(stage, "+C -C") == "");
  REQUIRE(format_sequence(stage.sequence()) == "");
  REQUIRE(apply_input(stage, "+ShiftLeft +D -D -ShiftLeft") == "+ShiftLeft -ShiftLeft");
  REQUIRE(format_sequence(stage.sequence()) == "");
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
  REQUIRE(format_sequence(stage.sequence()) == "");

  REQUIRE(apply_input(stage, "+B") == "+3");
  REQUIRE(apply_input(stage, "-B") == "-3");
  REQUIRE(format_sequence(stage.sequence()) == "");

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
  REQUIRE(format_sequence(stage.sequence()) == "");

  // CapsLock   =>
  REQUIRE(apply_input(stage, "+CapsLock") == "");
  REQUIRE(format_sequence(stage.sequence()) == "#CapsLock");
  REQUIRE(apply_input(stage, "-CapsLock") == "");
  REQUIRE(format_sequence(stage.sequence()) == "");

  // CapsLock{X}  =>
  REQUIRE(apply_input(stage, "+CapsLock") == "");
  REQUIRE(apply_input(stage, "+X") == "");
  REQUIRE(apply_input(stage, "-X") == "");
  REQUIRE(apply_input(stage, "-CapsLock") == "");
  REQUIRE(format_sequence(stage.sequence()) == "");

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
  REQUIRE(format_sequence(stage.sequence()) == "");
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
  REQUIRE(format_sequence(stage.sequence()) == "");

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
    Shift{X} >> 2 !Shift 1
  )";
  Stage stage = create_stage(config);

  // check that it temporarily released
  REQUIRE(apply_input(stage, "+ShiftLeft") == "+ShiftLeft");
  REQUIRE(format_sequence(stage.sequence()) == "#ShiftLeft");
  REQUIRE(apply_input(stage, "+X") == "+2 -ShiftLeft +1");
  REQUIRE(format_sequence(stage.sequence()) == "#ShiftLeft #X");
  REQUIRE(apply_input(stage, "+X") == "+ShiftLeft -2 +2 -ShiftLeft +1");
  REQUIRE(apply_input(stage, "+X") == "+ShiftLeft -2 +2 -ShiftLeft +1");
  REQUIRE(apply_input(stage, "-X") == "-2 -1");
  REQUIRE(apply_input(stage, "-ShiftLeft") == "");
  REQUIRE(format_sequence(stage.sequence()) == "");

  // check that it is reapplied
  REQUIRE(apply_input(stage, "+ShiftLeft") == "+ShiftLeft");
  REQUIRE(apply_input(stage, "+X") == "+2 -ShiftLeft +1");
  REQUIRE(apply_input(stage, "-X") == "-2 -1");
  REQUIRE(apply_input(stage, "+X") == "+ShiftLeft +2 -ShiftLeft +1");
  REQUIRE(apply_input(stage, "-X") == "-2 -1");
  REQUIRE(apply_input(stage, "+Y") == "+ShiftLeft +Y");
  REQUIRE(apply_input(stage, "-Y") == "-Y");
  REQUIRE(apply_input(stage, "-ShiftLeft") == "-ShiftLeft");
}

//--------------------------------------------------------------------

TEST_CASE("Toggle Virtual", "[Stage]") {
  auto config = R"(
    Boss       = Virtual1
    ScrollLock >> Boss
    Boss{A}    >> 1
  )";
  Stage stage = create_stage(config);

  REQUIRE(apply_input(stage, "+A") == "+A");
  REQUIRE(apply_input(stage, "-A") == "-A");

  REQUIRE(apply_input(stage, "+ScrollLock") == "");
  REQUIRE(apply_input(stage, "-ScrollLock") == "");
  REQUIRE(format_sequence(stage.sequence()) == "#Virtual1");

  REQUIRE(apply_input(stage, "+A") == "+1");
  REQUIRE(apply_input(stage, "-A") == "-1");

  REQUIRE(apply_input(stage, "+ScrollLock") == "");
  REQUIRE(apply_input(stage, "-ScrollLock") == "");
  REQUIRE(format_sequence(stage.sequence()) == "");

  REQUIRE(apply_input(stage, "+A") == "+A");
  REQUIRE(apply_input(stage, "-A") == "-A");
}

//--------------------------------------------------------------------

TEST_CASE("Press already pressed", "[Stage]") {
  auto config = R"(
    Shift{Quote} >> Shift{2}
  )";
  Stage stage = create_stage(config);

  REQUIRE(apply_input(stage, "+ShiftLeft") == "+ShiftLeft");
  REQUIRE(apply_input(stage, "+Quote") == "+ShiftLeft +2");
  REQUIRE(apply_input(stage, "-Quote") == "-2");
  REQUIRE(apply_input(stage, "+G") == "+G");
  REQUIRE(apply_input(stage, "-G") == "-G");
  REQUIRE(apply_input(stage, "-ShiftLeft") == "-ShiftLeft");
}

//--------------------------------------------------------------------

TEST_CASE("Press already pressed, with Not", "[Stage]") {
  auto config = R"(
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
  REQUIRE(format_sequence(stage.sequence()) == "");
}

//--------------------------------------------------------------------

TEST_CASE("Complex modifier - ordered", "[Stage]") {
  auto config = R"(
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
  REQUIRE(format_sequence(stage.sequence()) == "");

  REQUIRE(apply_input(stage, "+ShiftLeft") == "+ShiftLeft");
  REQUIRE(apply_input(stage, "+ControlLeft") == "+ControlLeft");
  REQUIRE(format_sequence(stage.sequence()) == "#ShiftLeft #ControlLeft");
  REQUIRE(apply_input(stage, "+I") == "+A");
  REQUIRE(apply_input(stage, "-I") == "-A");
  REQUIRE(apply_input(stage, "-ControlLeft") == "-ControlLeft");
  REQUIRE(apply_input(stage, "-ShiftLeft") == "-ShiftLeft");
  REQUIRE(format_sequence(stage.sequence()) == "");
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
    Meta{C} >> Control{C}
  )";
  Stage stage = create_stage(config);

  REQUIRE(apply_input(stage, "+MetaLeft") == "");
  REQUIRE(apply_input(stage, "+MetaLeft") == "");
  REQUIRE(apply_input(stage, "+C") == "+ControlLeft +C");
  REQUIRE(apply_input(stage, "-C") == "-ControlLeft -C");
  REQUIRE(apply_input(stage, "-MetaLeft") == "");

  REQUIRE(apply_input(stage, "+MetaLeft") == "");
  REQUIRE(apply_input(stage, "+MetaLeft") == "");
  REQUIRE(apply_input(stage, "+D") == "+MetaLeft +D");
  REQUIRE(apply_input(stage, "-D") == "-D");
  REQUIRE(apply_input(stage, "-MetaLeft") == "-MetaLeft");

  REQUIRE(apply_input(stage, "+MetaLeft") == "");
  REQUIRE(apply_input(stage, "+MetaLeft") == "");
  REQUIRE(apply_input(stage, "-MetaLeft") == "+MetaLeft -MetaLeft");
}

//--------------------------------------------------------------------

TEST_CASE("Any key", "[Stage]") {
  auto config = R"(
    Meta >> Meta
    Meta{Any} >> Any
    A >> B
    E >> F

    M A >> S
    M B >> Any
    M C >> !M Any

    K >> Any S
    X Y >> Any T
  )";
  Stage stage = create_stage(config);

  REQUIRE(apply_input(stage, "+A") == "+B");
  REQUIRE(apply_input(stage, "-A") == "-B");
  REQUIRE(apply_input(stage, "+E") == "+F");
  REQUIRE(apply_input(stage, "-E") == "-F");
  REQUIRE(apply_input(stage, "+H") == "+H");
  REQUIRE(apply_input(stage, "-H") == "-H");
  REQUIRE(format_sequence(stage.sequence()) == "");

  REQUIRE(apply_input(stage, "+MetaLeft") == "+MetaLeft");
  REQUIRE(apply_input(stage, "+A") == "+A");
  REQUIRE(apply_input(stage, "+E") == "+E");
  REQUIRE(apply_input(stage, "+H") == "+H");
  REQUIRE(apply_input(stage, "-A") == "-A");
  REQUIRE(apply_input(stage, "-E") == "-E");
  REQUIRE(apply_input(stage, "-H") == "-H");
  REQUIRE(apply_input(stage, "-MetaLeft") == "-MetaLeft");
  REQUIRE(format_sequence(stage.sequence()) == "");

  REQUIRE(apply_input(stage, "+K") == "+K +S");
  REQUIRE(apply_input(stage, "-K") == "-K -S");
  REQUIRE(format_sequence(stage.sequence()) == "");

  REQUIRE(apply_input(stage, "+X") == "");
  REQUIRE(apply_input(stage, "+Y") == "+X +Y +T");
  REQUIRE(apply_input(stage, "-X") == "");
  REQUIRE(apply_input(stage, "-Y") == "-X -Y -T");
}

//--------------------------------------------------------------------

TEST_CASE("Any key might match", "[Stage]") {
  auto config = R"(
    M A >> S
    M B >> Any
    M C >> !M Any

    N >> N
    N A >> S
    N B >> Any
    N C >> !N Any
  )";
  Stage stage = create_stage(config);

  REQUIRE(apply_input(stage, "+M") == "");
  REQUIRE(apply_input(stage, "+A") == "+S");
  REQUIRE(apply_input(stage, "-A") == "-S");
  REQUIRE(apply_input(stage, "-M") == "");

  REQUIRE(apply_input(stage, "+M") == "");
  REQUIRE(apply_input(stage, "+B") == "+M +B");
  REQUIRE(apply_input(stage, "-B") == "-M -B");
  REQUIRE(apply_input(stage, "-M") == "");

  REQUIRE(apply_input(stage, "+M") == "");
  REQUIRE(apply_input(stage, "+C") == "+C");
  REQUIRE(apply_input(stage, "-C") == "-C");
  REQUIRE(apply_input(stage, "-M") == "");
  REQUIRE(format_sequence(stage.sequence()) == "");

  REQUIRE(apply_input(stage, "+N") == "+N");
  REQUIRE(apply_input(stage, "+A") == "+S");
  REQUIRE(apply_input(stage, "-A") == "-S");
  REQUIRE(apply_input(stage, "-N") == "-N");

  REQUIRE(apply_input(stage, "+N") == "+N");
  REQUIRE(apply_input(stage, "+B") == "+B");
  REQUIRE(apply_input(stage, "-B") == "-B");
  REQUIRE(apply_input(stage, "-N") == "-N");

  REQUIRE(apply_input(stage, "+N") == "+N");
  REQUIRE(apply_input(stage, "+C") == "-N +C");
  REQUIRE(apply_input(stage, "-C") == "-C");
  REQUIRE(apply_input(stage, "-N") == "");
  REQUIRE(format_sequence(stage.sequence()) == "");
}

//--------------------------------------------------------------------
