
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

    [system='Linux' title=/firefox/i ] # comment
    CommandA >> Shift{Y}      # comment
    CommandB >> Shift{MyMacro}  # comment
  )";
  CHECK_NOTHROW(parse_config(string));
}

//--------------------------------------------------------------------

TEST_CASE("Problems", "[ParseConfig]") {
  // not mapped command
  auto string = R"(
    C >> CommandA
  )";
  CHECK_THROWS(parse_config(string));

  // duplicate command definition
  string = R"(
    C >> CommandA
    D >> CommandA
    CommandA >> E
  )";
  CHECK_THROWS(parse_config(string));

  // duplicate mapping definition
  string = R"(
    C >> CommandA
    CommandA >> D
    CommandA >> E
  )";
  CHECK_THROWS(parse_config(string));

  // unknown key/command
  string = R"(
    C >> CommandA
    CommandB >> E
  )";
  CHECK_THROWS(parse_config(string));

  // invalid declarative
  string = R"(
    C >> CommandA

    [windo]
    CommandA >> D
  )";
  CHECK_THROWS(parse_config(string));

  // empty declarative
  string = R"(
    C >> CommandA

    []
    CommandA >> D
  )";
  CHECK_THROWS(parse_config(string));

  // mapping not defined command
  string = R"(
    [class='']
    CommandB >> D
  )";
  CHECK_THROWS(parse_config(string));

  // duplicate mapping of command
  string = R"(
    C >> CommandA

    [class='']
    CommandA >> D
    CommandA >> E
  )";
  CHECK_THROWS(parse_config(string));

  // mapping sequence in context
  string = R"(
    [class='abc']
    C >> D
  )";
  CHECK_THROWS(parse_config(string));

  // defining command in context
  string = R"(
    [class='abc']
    C >> CommandA
  )";
  CHECK_THROWS(parse_config(string));

  // no default mapping (which is ok)
  string = R"(
    C >> CommandA

    [class='']
    CommandA >> D
  )";
  CHECK_NOTHROW(parse_config(string));

  // key after command name
  string = R"(
    C >> CommandA A
    CommandA >> D
  )";
  CHECK_THROWS(parse_config(string));

  // command name in sequence
  string = R"(
    C >> A CommandA
    CommandA >> D
  )";
  CHECK_THROWS(parse_config(string));

  // command after command name
  string = R"(
    C >> CommandA CommandB
    CommandA >> D
    CommandB >> E
  )";
  CHECK_THROWS(parse_config(string));
}

//--------------------------------------------------------------------

TEST_CASE("System contexts", "[ParseConfig]") {
  auto string = R"(
    A >> B
    B >> command

    [system="Linux"]
    command >> L

    [system="Linux" title="app1"]
    command >> X

    [system="Windows"]
    command >> W

    [system="Windows" title="app1"]
    command >> Y

    [title="app2"]
    command >> Z
  )";
  auto config = parse_config(string);
  auto commands = config.commands;
  REQUIRE(commands.size() == 2);
  REQUIRE(commands[0].context_mappings.empty());

  // other systems' and system only contexts were removed
  REQUIRE(config.contexts.size() == 2);
  REQUIRE(commands[1].context_mappings.size() == 2);
  REQUIRE(format_sequence(commands[0].default_mapping) == "+B");
#if defined(__linux__)
  REQUIRE(format_sequence(commands[1].default_mapping) == "+L");
#else
  REQUIRE(format_sequence(commands[1].default_mapping) == "+W");
#endif

  // context indices were updated
  auto app2_context = find_context(config, "Some", "app2");
  auto command1_mappings = config.commands[1].context_mappings;
  auto it = std::find_if(begin(command1_mappings), end(command1_mappings),
    [&](const ContextMapping& mapping) { return mapping.context_index == app2_context; });
  REQUIRE(it != end(command1_mappings));
  REQUIRE(format_sequence(it->output) == "+Z");
}

//--------------------------------------------------------------------

TEST_CASE("Context filters", "[ParseConfig]") {
  auto string = R"(
    A >> command

    [title = /Title1|Title2/ ]
    command >> B

    [title = /Title3/i]
    command >> C

    [title = "Title4"] # substring for titles
    command >> D

    [title = /^Title5$/]
    command >> E

    [class = /Class1|Class2/ ]
    command >> F

    [class = /Class3/i]
    command >> G

    [class = "Class4"] # exact string for classes
    command >> H

    [class = /^Class5$/]
    command >> I

    [class = /^Base\d+$/]
    command >> J
  )";
  auto config = parse_config(string);
  CHECK(find_context(config, "Some", "Title") == -1);
  CHECK(find_context(config, "Some", "Title1") == 0);
  CHECK(find_context(config, "Some", "Title2") == 0);
  CHECK(find_context(config, "Some", "title1") == -1);
  CHECK(find_context(config, "Some", "Title3") == 1);
  CHECK(find_context(config, "Some", "title3") == 1);
  CHECK(find_context(config, "Some", "Title4") == 2);
  CHECK(find_context(config, "Some", "_Title4_") == 2);
  CHECK(find_context(config, "Some", "title4") == -1);
  CHECK(find_context(config, "Some", "Title5") == 3);
  CHECK(find_context(config, "Some", "_Title5_") == -1);

  CHECK(find_context(config, "Class", "Some") == -1);
  CHECK(find_context(config, "Class1", "Some") == 4);
  CHECK(find_context(config, "Class2", "Some") == 4);
  CHECK(find_context(config, "class1", "Some") == -1);
  CHECK(find_context(config, "Class3", "Some") == 5);
  CHECK(find_context(config, "class3", "Some") == 5);
  CHECK(find_context(config, "Class4", "Some") == 6);
  CHECK(find_context(config, "_Class4_", "Some") == -1);
  CHECK(find_context(config, "class4", "Some") == -1);
  CHECK(find_context(config, "Class5", "Some") == 7);
  CHECK(find_context(config, "_Class5_", "Some") == -1);
  CHECK(find_context(config, "Base100", "Some") == 8);
  CHECK(find_context(config, "Base100_", "Some") == -1);

  CHECK(config.contexts[0].window_title_filter.string == "/Title1|Title2/");
  CHECK(config.contexts[6].window_class_filter.string == "Class4");
  CHECK(config.contexts[7].window_class_filter.string == "/^Class5$/");
}

//--------------------------------------------------------------------

TEST_CASE("Macros", "[ParseConfig]") {
  // correct
  auto string = R"(
    MyMacro = A B C
    MyMacro >> B
    C >> MyMacro
  )";
  CHECK_NOTHROW(parse_config(string));

  // not allowed macro name
  string = R"(
    Space = Enter
  )";
  CHECK_THROWS(parse_config(string));
}

//--------------------------------------------------------------------

TEST_CASE("Old and new context format", "[ParseConfig]") {
  auto string = R"(
    [window class='test' title=test]
    [Window class='test' title=test]
    [class='test' title=test]
  )";
  CHECK_NOTHROW(parse_config(string));
}

//--------------------------------------------------------------------

