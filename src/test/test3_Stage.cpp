
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

  // +LeftShift => +LeftShift
  REQUIRE(apply_input(stage, "+LeftShift") == "+LeftShift");

  // +A +A +A -A => +B +B +B -B
  REQUIRE(apply_input(stage, "+A") == "+B");
  REQUIRE(apply_input(stage, "+A") == "+B");
  REQUIRE(apply_input(stage, "+A") == "+B");
  REQUIRE(apply_input(stage, "-A") == "-B");

  // -LShift => -LShift
  REQUIRE(apply_input(stage, "-LeftShift") == "-LeftShift");
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

  // M =>
  REQUIRE(apply_input(stage, "+M") == "");
  REQUIRE(apply_input(stage, "-M") == "");

  // M => M M
  REQUIRE(apply_input(stage, "+M") == "+M -M +M");
  REQUIRE(format_sequence(stage.sequence()) == "#M");
  REQUIRE(apply_input(stage, "-M") == "-M");
  REQUIRE(format_sequence(stage.sequence()) == "");

  // +M R  =>  A
  REQUIRE(apply_input(stage, "+M") == "");
  REQUIRE(apply_input(stage, "+R") == "+A");
  REQUIRE(apply_input(stage, "-R") == "-A");
  REQUIRE(format_sequence(stage.sequence()) == "#M");

  // S  =>  B
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
    Ctrl{K} X   >> 1
    Ctrl{K} ANY >>
  )";
  Stage stage = create_stage(config);

  // Ctrl{K} X => 1
  REQUIRE(apply_input(stage, "+LeftCtrl") == "+LeftCtrl");
  REQUIRE(apply_input(stage, "+K") == "");
  REQUIRE(apply_input(stage, "-K") == "");
  REQUIRE(apply_input(stage, "-LeftCtrl") == "-LeftCtrl");
  REQUIRE(apply_input(stage, "+X") == "+1");
  REQUIRE(apply_input(stage, "-X") == "-1");

  // K => K
  REQUIRE(apply_input(stage, "+K") == "+K");
  REQUIRE(apply_input(stage, "-K") == "-K");

  // X => X
  REQUIRE(apply_input(stage, "+X") == "+X");
  REQUIRE(apply_input(stage, "-X") == "-X");

  // Ctrl{K} Y =>
  REQUIRE(apply_input(stage, "+LeftCtrl") == "+LeftCtrl");
  REQUIRE(apply_input(stage, "+K -K") == "");
  REQUIRE(apply_input(stage, "-LeftCtrl") == "-LeftCtrl");
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

  // B C LeftShift{D} => LeftShift
  REQUIRE(apply_input(stage, "+B -B") == "");
  REQUIRE(format_sequence(stage.sequence()) == "");
  REQUIRE(apply_input(stage, "+C -C") == "");
  REQUIRE(format_sequence(stage.sequence()) == "");
  REQUIRE(apply_input(stage, "+LeftShift +D -D -LeftShift") == "+LeftShift -LeftShift");
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
    Ext         = 102ND
    Ext         >>
    Ext{I}      >> Up
    Ext{K}      >> Down
    Ext{J}      >> Left
    Ext{L}      >> Right
    Ext{D}      >> Shift
    Ext{Any}    >>
  )";
  Stage stage = create_stage(config);

  // I   =>
  REQUIRE(apply_input(stage, "+I") == "+I");
  REQUIRE(format_sequence(stage.sequence()) == "#I");
  REQUIRE(apply_input(stage, "-I") == "-I");
  REQUIRE(format_sequence(stage.sequence()) == "");

  // 102ND   =>
  REQUIRE(apply_input(stage, "+102ND") == "");
  REQUIRE(format_sequence(stage.sequence()) == "#102ND");
  REQUIRE(apply_input(stage, "-102ND") == "");
  REQUIRE(format_sequence(stage.sequence()) == "");

  // 102ND{I}  => Up
  REQUIRE(apply_input(stage, "+102ND") == "");
  REQUIRE(format_sequence(stage.sequence()) == "#102ND");

  REQUIRE(apply_input(stage, "+I") == "+Up");
  REQUIRE(format_sequence(stage.sequence()) == "#102ND #I");
  REQUIRE(apply_input(stage, "+I") == "+Up");
  REQUIRE(format_sequence(stage.sequence()) == "#102ND #I");
  REQUIRE(apply_input(stage, "-I") == "-Up");
  REQUIRE(format_sequence(stage.sequence()) == "#102ND");

  // (102ND D){I}  => LeftShift{Up}
  REQUIRE(apply_input(stage, "+D") == "+LeftShift");
  REQUIRE(format_sequence(stage.sequence()) == "#102ND #D");

  REQUIRE(apply_input(stage, "+I") == "+Up");
  REQUIRE(format_sequence(stage.sequence()) == "#102ND #D #I");
  REQUIRE(apply_input(stage, "+I") == "+Up");
  REQUIRE(format_sequence(stage.sequence()) == "#102ND #D #I");
  REQUIRE(apply_input(stage, "-I") == "-Up");
  REQUIRE(format_sequence(stage.sequence()) == "#102ND #D");
  REQUIRE(apply_input(stage, "-D") == "-LeftShift");
  REQUIRE(format_sequence(stage.sequence()) == "#102ND");

  REQUIRE(apply_input(stage, "-102ND") == "");
  REQUIRE(format_sequence(stage.sequence()) == "");
}

//--------------------------------------------------------------------

TEST_CASE("Any matches any key", "[Stage]") {
  auto config = R"(
    A{B} >> 1
    ANY  >>
  )";
  Stage stage = create_stage(config);

  REQUIRE(apply_input(stage, "+A -A") == "");

  REQUIRE(apply_input(stage, "+B -B") == "");

  REQUIRE(apply_input(stage, "+A") == "");
  REQUIRE(apply_input(stage, "+B") == "+1");
  REQUIRE(apply_input(stage, "-B") == "-1");
  REQUIRE(apply_input(stage, "-A") == "");
}

//--------------------------------------------------------------------

TEST_CASE("Not in output", "[Stage]") {
  auto config = R"(
    Ctrl{X} >> !Ctrl 1
  )";
  Stage stage = create_stage(config);

  REQUIRE(apply_input(stage, "+LeftCtrl") == "+LeftCtrl");
  REQUIRE(apply_input(stage, "-LeftCtrl") == "-LeftCtrl");
  REQUIRE(format_sequence(stage.sequence()) == "");

  REQUIRE(apply_input(stage, "+X") == "+X");
  REQUIRE(apply_input(stage, "-X") == "-X");
  REQUIRE(format_sequence(stage.sequence()) == "");

  // check that it temporarily released
  REQUIRE(apply_input(stage, "+LeftCtrl") == "+LeftCtrl");
  REQUIRE(format_sequence(stage.sequence()) == "#LeftCtrl");
  REQUIRE(apply_input(stage, "+X") == "-LeftCtrl +1");
  REQUIRE(format_sequence(stage.sequence()) == "#LeftCtrl #X");
  REQUIRE(apply_input(stage, "-X") == "-1");
  REQUIRE(apply_input(stage, "-LeftCtrl") == "");
  REQUIRE(format_sequence(stage.sequence()) == "");

  // check that it is reapplied
  REQUIRE(apply_input(stage, "+LeftCtrl") == "+LeftCtrl");
  REQUIRE(apply_input(stage, "+X") == "-LeftCtrl +1");
  REQUIRE(apply_input(stage, "-X") == "-1");
  REQUIRE(apply_input(stage, "+Y") == "+LeftCtrl +Y");
  REQUIRE(apply_input(stage, "-Y") == "-Y");
  REQUIRE(apply_input(stage, "-LeftCtrl") == "-LeftCtrl");
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
    Shift{Apostrophe} >> Shift{2}
  )";
  Stage stage = create_stage(config);

  REQUIRE(apply_input(stage, "+LeftShift") == "+LeftShift");
  REQUIRE(apply_input(stage, "+Apostrophe") == "+LeftShift +2");
  REQUIRE(apply_input(stage, "-Apostrophe") == "-2");
  REQUIRE(apply_input(stage, "+G") == "+G");
  REQUIRE(apply_input(stage, "-G") == "-G");
  REQUIRE(apply_input(stage, "-LeftShift") == "-LeftShift");
}

//--------------------------------------------------------------------

TEST_CASE("Complex modifier - ordered", "[Stage]") {
  auto config = R"(
    Ctrl{W{I}} >> A
  )";
  Stage stage = create_stage(config);

  REQUIRE(apply_input(stage, "+W") == "+W");
  REQUIRE(apply_input(stage, "-W") == "-W");

  REQUIRE(apply_input(stage, "+LeftCtrl") == "+LeftCtrl");
  REQUIRE(apply_input(stage, "+W") == "");
  REQUIRE(apply_input(stage, "+I") == "+A");
  REQUIRE(apply_input(stage, "-I") == "-A");
  REQUIRE(apply_input(stage, "-W") == "");

  REQUIRE(apply_input(stage, "+I") == "+I");
  REQUIRE(apply_input(stage, "-I") == "-I");
  REQUIRE(apply_input(stage, "-LeftCtrl") == "-LeftCtrl");

  REQUIRE(apply_input(stage, "+W") == "+W");
  REQUIRE(apply_input(stage, "+LeftCtrl") == "+LeftCtrl");
  REQUIRE(apply_input(stage, "+I") == "+I");
  REQUIRE(apply_input(stage, "-I") == "-I");
  REQUIRE(apply_input(stage, "-W") == "-W");
  REQUIRE(apply_input(stage, "-LeftCtrl") == "-LeftCtrl");
}

//--------------------------------------------------------------------

TEST_CASE("Complex modifier - unordered", "[Stage]") {
  auto config = R"(
    (Ctrl Shift){I} >> A
  )";
  Stage stage = create_stage(config);

  REQUIRE(apply_input(stage, "+LeftShift") == "+LeftShift");
  REQUIRE(apply_input(stage, "-LeftShift") == "-LeftShift");

  REQUIRE(apply_input(stage, "+LeftCtrl") == "+LeftCtrl");
  REQUIRE(apply_input(stage, "+LeftShift") == "+LeftShift");
  REQUIRE(format_sequence(stage.sequence()) == "#LeftCtrl #LeftShift");
  REQUIRE(apply_input(stage, "+I") == "+A");
  REQUIRE(apply_input(stage, "-I") == "-A");
  REQUIRE(apply_input(stage, "-LeftShift") == "-LeftShift");
  REQUIRE(format_sequence(stage.sequence()) == "#LeftCtrl");

  REQUIRE(apply_input(stage, "+I") == "+I");
  REQUIRE(apply_input(stage, "-I") == "-I");
  REQUIRE(apply_input(stage, "-LeftCtrl") == "-LeftCtrl");
  REQUIRE(format_sequence(stage.sequence()) == "");

  REQUIRE(apply_input(stage, "+LeftShift") == "+LeftShift");
  REQUIRE(apply_input(stage, "+LeftCtrl") == "+LeftCtrl");
  REQUIRE(format_sequence(stage.sequence()) == "#LeftShift #LeftCtrl");
  REQUIRE(apply_input(stage, "+I") == "+A");
  REQUIRE(apply_input(stage, "-I") == "-A");
  REQUIRE(apply_input(stage, "-LeftCtrl") == "-LeftCtrl");
  REQUIRE(apply_input(stage, "-LeftShift") == "-LeftShift");
  REQUIRE(format_sequence(stage.sequence()) == "");
}

//--------------------------------------------------------------------
