
#include "test.h"
#include "config/ParseConfig.h"

namespace {
  Config parse_config(const char* config) {
    static auto parse = ParseConfig();
    auto stream = std::stringstream(config);
    return parse(stream, false);
  }
} // namespace

//--------------------------------------------------------------------

TEST_CASE("Valid config", "[ParseConfig]") {
  auto string = R"(
    # comment
    MyMacro = A B C# comment

    Shift{A} >> B
    C >> CommandA
    CommandA >> X
    E >> CommandB

    # comment
    [ system = "Windows" class='test'title=test ] # comment
    CommandA >> Y        # comment
    CommandB >> MyMacro    # comment

    [system='Linux'] # comment
    CommandA >> Shift{Y}      # comment
    CommandB >> Shift{MyMacro}  # comment
  )";
  REQUIRE_NOTHROW(parse_config(string));
}

//--------------------------------------------------------------------

TEST_CASE("Problems", "[ParseConfig]") {
  // not mapped command
  auto string = R"(
    C >> CommandA
  )";
  REQUIRE_THROWS(parse_config(string));

  // duplicate command definition
  string = R"(
    C >> CommandA
    D >> CommandA
    CommandA >> E
  )";
  REQUIRE_THROWS(parse_config(string));

  // duplicate mapping definition
  string = R"(
    C >> CommandA
    CommandA >> D
    CommandA >> E
  )";
  REQUIRE_THROWS(parse_config(string));

  // unknown key/command
  string = R"(
    C >> CommandA
    CommandB >> E
  )";
  REQUIRE_THROWS(parse_config(string));

  // invalid declarative
  string = R"(
    C >> CommandA

    [windo]
    CommandA >> D
  )";
  REQUIRE_THROWS(parse_config(string));

  // empty declarative
  string = R"(
    C >> CommandA

    []
    CommandA >> D
  )";
  REQUIRE_THROWS(parse_config(string));

  // mapping not defined command
  string = R"(
    [class='']
    CommandB >> D
  )";
  REQUIRE_THROWS(parse_config(string));

  // duplicate mapping of command
  string = R"(
    C >> CommandA

    [class='']
    CommandA >> D
    CommandA >> E
  )";
  REQUIRE_THROWS(parse_config(string));

  // mapping sequence in context
  string = R"(
    [class='abc']
    C >> D
  )";
  REQUIRE_THROWS(parse_config(string));

  // defining command in context
  string = R"(
    [class='abc']
    C >> CommandA
  )";
  REQUIRE_THROWS(parse_config(string));

  // no default mapping (which is ok)
  string = R"(
    C >> CommandA

    [class='']
    CommandA >> D
  )";
  REQUIRE_NOTHROW(parse_config(string));
}

//--------------------------------------------------------------------

TEST_CASE("System contexts", "[ParseConfig]") {
  auto string = R"(
    A >> B
    B >> command

    [system="Linux"]
    command >> L

    [system="Linux" title="app"]
    command >> X

    [system="Windows"]
    command >> W

    [system="Windows" title="app"]
    command >> Y
  )";
  auto config = parse_config(string);
  REQUIRE(config.contexts.size() == 1);
  auto commands = config.commands;
  REQUIRE(commands.size() == 2);
  REQUIRE(commands[0].context_mappings.empty());
  REQUIRE(commands[1].context_mappings.size() == 1);
  REQUIRE(format_sequence(commands[0].default_mapping) == "+B");
#if defined(__linux__)
  REQUIRE(format_sequence(commands[1].default_mapping) == "+L");
#else
  REQUIRE(format_sequence(commands[1].default_mapping) == "+W");
#endif
}

//--------------------------------------------------------------------

TEST_CASE("Macros", "[ParseConfig]") {
  // correct
  auto string = R"(
    MyMacro = A B C
    MyMacro >> B
    C >> MyMacro
  )";
  REQUIRE_NOTHROW(parse_config(string));

  // not allowed macro name
  string = R"(
    Space = Enter
  )";
  REQUIRE_THROWS(parse_config(string));
}

//--------------------------------------------------------------------

TEST_CASE("Old and new context format", "[ParseConfig]") {
  auto string = R"(
    [window class='test' title=test]
    [Window class='test' title=test]
    [class='test' title=test]
  )";
  REQUIRE_NOTHROW(parse_config(string));
}

//--------------------------------------------------------------------

