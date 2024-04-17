
#include "test.h"
#include "config/ParseConfig.h"

namespace {
  Config parse_config(const char* config) {
    static auto parse = ParseConfig();
    auto stream = std::stringstream(config);
    return parse(stream);
  }

  int find_context(const Config& config, 
      const char* window_class, 
      const char* window_title, 
      const char* window_path = "") {
    const auto& contexts = config.contexts;
    // skip default context
    const auto begin = std::next(contexts.begin());
    const auto end = contexts.end();
    const auto it = std::find_if(begin, end,
      [&](const Config::Context& context) {
        return context.matches(window_class, window_title, window_path);
      });
    return static_cast<int>(it == end ? 0 : std::distance(begin, it) + 1);
  }
} // namespace

//--------------------------------------------------------------------

TEST_CASE("Valid config", "[ParseConfig]") {
  auto string = R"(
    # comment
    MyMacro = A B C# comment

    Shift{A} >> B
    C >> CommandA ; comment
    CommandA >> X
    E >> CommandB

    # comment
    [ system = "Windows" class='test'title=test ] # comment
    CommandA >> Y        # comment
    CommandB >> MyMacro    # comment

    [system='Linux' title=/firefox[123]*x{1,3}/i ] # comment
    CommandA >> Shift{Y}      # comment
    CommandB >> Shift{MyMacro}  # comment
  )";
  CHECK_NOTHROW(parse_config(string));
}

//--------------------------------------------------------------------

TEST_CASE("Problems", "[ParseConfig]") {
  // input without Down
  CHECK_THROWS(parse_config(R"(!A >> B)"));  
  CHECK_THROWS(parse_config(R"(!A 100ms >> B)"));  

  // not mapped command
  auto string = R"(
    C >> CommandA
  )";
  CHECK_THROWS(parse_config(string));

  // duplicate command definition (which is ok)
  string = R"(
    C >> CommandA
    D >> CommandA
    CommandA >> E
  )";
  CHECK_NOTHROW(parse_config(string));

  // duplicate mapping definition
  string = R"(
    C >> CommandA
    CommandA >> D
    CommandA >> E
  )";
  CHECK_THROWS(parse_config(string));

  // unknown key/command
  string = R"(
    CommandB >> E
  )";
  CHECK_THROWS(parse_config(string));

  // mapping command to command
  string = R"(
    C >> CommandA
    CommandA >> CommandB
    CommandB >> D
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

  // mapping sequence in context (which is ok)
  string = R"(
    [class='abc']
    C >> D
  )";
  CHECK_NOTHROW(parse_config(string));

  // defining command in context (which is ok)
  string = R"(
    [class='abc']
    C >> CommandA
    CommandA >> D
  )";
  CHECK_NOTHROW(parse_config(string));

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

  // missing ]
  string = R"(
    C >> CommandA
    [system='Linux'
    CommandA >> D
  )";
  CHECK_THROWS(parse_config(string));

  // character after context block
  string = R"(
    C >> CommandA
    [system='Linux'] a
    CommandA >> D
  )";
  CHECK_THROWS(parse_config(string));

  // regex for system
  string = R"(
    C >> CommandA
    [system=/Linux/]
    CommandA >> D
  )";
  CHECK_THROWS(parse_config(string));

  // invalid regex
  string = R"(
    C >> CommandA
    [class=/Linux(/]
    CommandA >> D
  )";
  CHECK_THROWS(parse_config(string));

  // context filters can optionally be separted with commas
  string = R"(
    [class = 'abc', title = "test"]
    A >> B
  )";
  CHECK_NOTHROW(parse_config(string));

  string = R"(
    [class = 'abc',, title = "test"]
    A >> B
  )";
  CHECK_THROWS(parse_config(string));

  string = R"(
    [class = 'abc', title = "test",]
    A >> B
  )";
  CHECK_THROWS(parse_config(string));
}

//--------------------------------------------------------------------

TEST_CASE("System contexts", "[ParseConfig]") {
  auto string = R"(
    [default]
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

    [system="MacOS"]
    command >> M

    [system="MacOS" title="app1"]
    command >> Z

    [title="app2"]
    command >> R
  )";
  auto config = parse_config(string);

  // other systems' contexts were removed
  REQUIRE(config.contexts.size() == 4);
  REQUIRE(config.contexts[0].inputs.size() == 2);
  REQUIRE(config.contexts[0].outputs.size() == 1);
  REQUIRE(config.contexts[0].command_outputs.size() == 0);

  for (auto i = 1; i < 3; ++i) {
    REQUIRE(config.contexts[i].inputs.size() == 0);
    REQUIRE(config.contexts[i].outputs.size() == 0);
    REQUIRE(config.contexts[i].command_outputs.size() == 1);
  }
  REQUIRE(format_sequence(config.contexts[0].outputs[0]) == "+B");

#if defined(__linux__)
  REQUIRE(format_sequence(config.contexts[1].command_outputs[0].output) == "+L");
  REQUIRE(format_sequence(config.contexts[2].command_outputs[0].output) == "+X");
#elif defined(_WIN32)
  REQUIRE(format_sequence(config.contexts[1].command_outputs[0].output) == "+W");
  REQUIRE(format_sequence(config.contexts[2].command_outputs[0].output) == "+Y");
#else
  REQUIRE(format_sequence(config.contexts[1].command_outputs[0].output) == "+M");
  REQUIRE(format_sequence(config.contexts[2].command_outputs[0].output) == "+Z");
#endif

  REQUIRE(format_sequence(config.contexts[3].command_outputs[0].output) == "+R");
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
  CHECK(find_context(config, "Some", "Title") == 0);
  CHECK(find_context(config, "Some", "Title1") == 1);
  CHECK(find_context(config, "Some", "Title2") == 1);
  CHECK(find_context(config, "Some", "title1") == 0);
  CHECK(find_context(config, "Some", "Title3") == 2);
  CHECK(find_context(config, "Some", "title3") == 2);
  CHECK(find_context(config, "Some", "Title4") == 3);
  CHECK(find_context(config, "Some", "_Title4_") == 3);
  CHECK(find_context(config, "Some", "title4") == 0);
  CHECK(find_context(config, "Some", "Title5") == 4);
  CHECK(find_context(config, "Some", "_Title5_") == 0);

  CHECK(find_context(config, "Class", "Some") == 0);
  CHECK(find_context(config, "Class1", "Some") == 5);
  CHECK(find_context(config, "Class2", "Some") == 5);
  CHECK(find_context(config, "class1", "Some") == 0);
  CHECK(find_context(config, "Class3", "Some") == 6);
  CHECK(find_context(config, "class3", "Some") == 6);
  CHECK(find_context(config, "Class4", "Some") == 7);
  CHECK(find_context(config, "_Class4_", "Some") == 0);
  CHECK(find_context(config, "class4", "Some") == 0);
  CHECK(find_context(config, "Class5", "Some") == 8);
  CHECK(find_context(config, "_Class5_", "Some") == 0);
  CHECK(find_context(config, "Base100", "Some") == 9);
  CHECK(find_context(config, "Base100_", "Some") == 0);

  CHECK(config.contexts[1].window_title_filter.string == "/Title1|Title2/");
  CHECK(config.contexts[7].window_class_filter.string == "Class4");
  CHECK(config.contexts[8].window_class_filter.string == "/^Class5$/");
}

//--------------------------------------------------------------------

TEST_CASE("Context modifier", "[ParseConfig]") {
  auto string = R"(
    Ext = A
    Ext{C} >> X

    [modifier = "Ext"]
    D >> Y

    [modifier = "!Ext"]
    E >> Z

    [modifier = "Virtual0 !Virtual1"]
    F >> W
  )";
  auto config = parse_config(string);
  REQUIRE(config.contexts.size() == 4);
  REQUIRE(config.contexts[0].inputs.size() == 1);
  REQUIRE(config.contexts[1].inputs.size() == 1);
  REQUIRE(config.contexts[2].inputs.size() == 1);
  REQUIRE(config.contexts[3].inputs.size() == 1);
  CHECK(format_sequence(config.contexts[0].inputs[0].input) == "+A +C ~C ~A");
  CHECK(format_sequence(config.contexts[0].modifier_filter) == "");
  CHECK(format_sequence(config.contexts[1].inputs[0].input) == "+D ~D");
  CHECK(format_sequence(config.contexts[1].modifier_filter) == "+A");
  CHECK(format_sequence(config.contexts[2].inputs[0].input) == "+E ~E");
  CHECK(format_sequence(config.contexts[2].modifier_filter) == "!A");
  CHECK(format_sequence(config.contexts[3].inputs[0].input) == "+F ~F");
  CHECK(format_sequence(config.contexts[3].modifier_filter) == "+Virtual0 !Virtual1");
}

//--------------------------------------------------------------------

TEST_CASE("Context macro", "[ParseConfig]") {
  auto string = R"(
    DeviceA = /Device1/
    TitleB = "Title1"

    [device=DeviceA class = "A"]
    A >> B

    [title=TitleB device = DeviceB]
    E >> F
  )";

  auto config = parse_config(string);
  REQUIRE(config.contexts.size() == 2);
  CHECK(config.contexts[0].device_filter == "/Device1/");
  CHECK(config.contexts[0].window_class_filter.string == "A");
  CHECK(config.contexts[1].device_filter == "DeviceB");
  CHECK(config.contexts[1].window_title_filter.string == "Title1");
}

//--------------------------------------------------------------------

TEST_CASE("Context with inverted filters", "[ParseConfig]") {
  auto string = R"(
    [device != DeviceA class!="A" title!="B" path!="C" modifier!=A]
    A >> B
  )";

  auto config = parse_config(string);
  REQUIRE(config.contexts.size() == 1);
  CHECK(config.contexts[0].invert_device_filter);
  CHECK(config.contexts[0].invert_modifier_filter);
  CHECK(config.contexts[0].window_title_filter.invert);
  CHECK(config.contexts[0].window_class_filter.invert);
  CHECK(config.contexts[0].window_path_filter.invert);
}

//--------------------------------------------------------------------

TEST_CASE("Context with fallthrough", "[ParseConfig]") {
  auto string = R"(
    [title = A]
    A >> B

    [title = B]  # fallthrough
    [title = C]
    A >> B

    [title = D]  # removed
    
    [title = E]
    A >> B

    [title = F]  # removed
    #A >> B
    [title = G]
    A >> B

    [title = H]  # removed
    [title = I]  # removed

    [title = J]  # fallthrough
    [system = "Linux"]   # two of
    [system = "Windows"] # three
    [system = "MacOS"]   # are removed
    A >> B
  )";

  auto config = parse_config(string);
  REQUIRE(config.contexts.size() == 7);
  CHECK(!config.contexts[0].fallthrough); // A
  CHECK(config.contexts[1].fallthrough);  // B fallthrough
  CHECK(!config.contexts[2].fallthrough); // C
  CHECK(!config.contexts[3].fallthrough); // E
  CHECK(!config.contexts[4].fallthrough); // G
  CHECK(config.contexts[5].fallthrough);  // J fallthrough
  CHECK(!config.contexts[6].fallthrough); // System
}

//--------------------------------------------------------------------

TEST_CASE("Macros", "[ParseConfig]") {
  auto string = R"(
    MyMacro = A{B}
    MyMacro >> C # MyMacro
    C >> MyMacro   # MyMacro
  )";
  auto config = Config{ };
  REQUIRE_NOTHROW(config = parse_config(string));
  REQUIRE(config.contexts[0].inputs.size() == 2);
  REQUIRE(config.contexts[0].outputs.size() == 2);
  REQUIRE(config.contexts[0].command_outputs.size() == 0);
  CHECK(format_sequence(config.contexts[0].inputs[0].input) == "+A +B ~B ~A");
  CHECK(format_sequence(config.contexts[0].outputs[0]) == "+C");
  CHECK(format_sequence(config.contexts[0].inputs[1].input) == "+C ~C");
  CHECK(format_sequence(config.contexts[0].outputs[1]) == "+A +B -B -A");

  string = R"(
    Macro1 = F   # >
    Macro2 = E Macro1 G  # Macro1
    Macro3 =     # -
    Macro1 A Macro2 Macro3 >> Macro3 Macro2 B Macro1
  )";
  REQUIRE_NOTHROW(config = parse_config(string));
  REQUIRE(config.contexts[0].inputs.size() == 1);
  REQUIRE(config.contexts[0].outputs.size() == 1);
  REQUIRE(config.contexts[0].command_outputs.size() == 0);
  CHECK(format_sequence(config.contexts[0].inputs[0].input) == "+F ~F +A ~A +E ~E +F ~F +G ~G");
  CHECK(format_sequence(config.contexts[0].outputs[0]) == "+E -E +F -F +G -G +B -B +F -F");

  // do not substitute in string
  string = R"(
    ab = E F
    X >> ab 'ab' ab
  )";
  REQUIRE_NOTHROW(config = parse_config(string));
  REQUIRE(config.contexts[0].inputs.size() == 1);
  REQUIRE(config.contexts[0].outputs.size() == 1);
  REQUIRE(config.contexts[0].command_outputs.size() == 0);
  CHECK(format_sequence(config.contexts[0].inputs[0].input) == "+X ~X");
  CHECK(format_sequence(config.contexts[0].outputs[0]) == 
    "+E -E +F -F !MetaLeft !MetaRight !ShiftLeft !ShiftRight !AltLeft !AltRight !ControlLeft !ControlRight +A -A +B -B +E -E +F -F");

  // not allowed macro name
  string = R"(
    Space = Enter
  )";
  CHECK_THROWS(parse_config(string));
}

//--------------------------------------------------------------------

TEST_CASE("Macros and system filter", "[ParseConfig]") {
  auto string = R"(
    [system="Linux"]
    Macro1 = C

    [system="Windows"]
    Macro1 = D

    [system="MacOS"]
    Macro1 = E

    [system="MacOS"]
    Macro2 = F

    [system="Windows"]
    Macro2 = G

    [system="Linux"]
    Macro2 = H

    [default]
    Macro1 >> X
    Macro2 >> Y
  )";
  auto config = parse_config(string);

  // contexts without mappings are removed
  REQUIRE(config.contexts.size() == 1);
  REQUIRE(config.contexts[0].inputs.size() == 2);

#if defined(__linux__)
  CHECK(format_sequence(config.contexts[0].inputs[0].input) == "+C ~C");
  CHECK(format_sequence(config.contexts[0].inputs[1].input) == "+H ~H");
#elif defined(_WIN32)
  CHECK(format_sequence(config.contexts[0].inputs[0].input) == "+D ~D");
  CHECK(format_sequence(config.contexts[0].inputs[1].input) == "+G ~G");
#else
  CHECK(format_sequence(config.contexts[0].inputs[0].input) == "+E ~E");
  CHECK(format_sequence(config.contexts[0].inputs[1].input) == "+F ~F");
#endif

  // macro not defined on system
  string = R"(
    [system="Linux"]
    Macro1 = C

    [system="Windows"]
    Macro2 = D

    [system="MacOS"]
    Macro3 = E

    [default]
    Macro1 >> X
    Macro2 >> Y
    Macro3 >> Y
  )";
  CHECK_THROWS(parse_config(string));
}

//--------------------------------------------------------------------

TEST_CASE("Macros with arguments", "[ParseConfig]") {
  auto string = R"(
    modify = $0{ $1 }
    print = "$0"
    echo = $(echo "$0")
    func = modify[$0 $2, $1]

    A >> modify[ShiftLeft, X]
    B >> print[X]
    C >> print[ "a b" ]
    E >> echo["Title"]
    F >> func[X, Y, Z, W]
    G >> func[X, Y]
    H >> func[ , Y, (Z W)]
    I >> func[ , Y Z, W]
  )";
  auto config = parse_config(string);

  REQUIRE(config.contexts.size() == 1);
  REQUIRE(config.contexts[0].inputs.size() == 8);
  REQUIRE(config.contexts[0].outputs.size() == 8);

  CHECK(format_sequence(config.contexts[0].outputs[0]) == "+ShiftLeft +X -X -ShiftLeft");

  CHECK(format_sequence(config.contexts[0].outputs[1]) ==
    "!MetaLeft !MetaRight +ShiftLeft !AltLeft !AltRight !ControlLeft !ControlRight "
    "+X -X -ShiftLeft");

  CHECK(format_sequence(config.contexts[0].outputs[2]) ==
    "!MetaLeft !MetaRight !ShiftLeft !ShiftRight !AltLeft !AltRight !ControlLeft !ControlRight "
    "+A -A +Space -Space +B -B");

  CHECK(format_sequence(config.contexts[0].outputs[3]) == "+Action0");

  CHECK(format_sequence(config.contexts[0].outputs[4]) == "+X -X +Z +Y -Y -Z");
  CHECK(format_sequence(config.contexts[0].outputs[5]) == "+X +Y -Y -X");
  CHECK(format_sequence(config.contexts[0].outputs[6]) == "+Z +W +Y -Y -W -Z");
  CHECK(format_sequence(config.contexts[0].outputs[7]) == "+W +Y -Y +Z -Z -W");
}

//--------------------------------------------------------------------

TEST_CASE("Macros with arguments - Problems", "[ParseConfig]") {
  CHECK_NOTHROW(parse_config(R"( 
    macro = $0 
    A >> macro[]
  )"));
  CHECK_NOTHROW(parse_config(R"( 
    macro = $0 
    A >> macro[,]
  )"));
  CHECK_THROWS(parse_config(R"( 
    macro = $0 
    A >> macro["]
  )"));
  CHECK_THROWS(parse_config(R"( 
    macro = $0 
    A >> macro[']
  )"));
  CHECK_THROWS(parse_config(R"( 
    macro = $0 
    A >> macro[,']
  )"));
  CHECK_THROWS(parse_config(R"( 
    macro = $0 
    A >> macro[$]
  )"));
}

//--------------------------------------------------------------------

TEST_CASE("Macros with Alias arguments", "[ParseConfig]") {
  auto string = R"(
    twice = $0 $0
    Boss = Virtual1
    A >> twice[X]
    B >> twice[Boss]
  )";
  auto config = parse_config(string);
  REQUIRE(config.contexts.size() == 1);
  REQUIRE(config.contexts[0].outputs.size() == 2);
  CHECK(format_sequence(config.contexts[0].outputs[0]) == "+X -X +X -X");
  CHECK(format_sequence(config.contexts[0].outputs[1]) == "+Virtual1 -Virtual1 +Virtual1 -Virtual1");
}

//--------------------------------------------------------------------

TEST_CASE("Macros with Terminal Commands", "[ParseConfig]") {
  auto string = R"(
    notify = $(notify-send -t 2000 -a "keymapper" "$0")
    F1 >> notify["F7"]

    echo = $(echo $0$1)
    F2 >> echo[echo]
    F3 >> echo[" echo "]
    F4 >> echo["echo, echo"]
    F5 >> echo["echo", " echo "]
    F6 >> $(echo "echo")
  )";
  auto config = parse_config(string);
  REQUIRE(config.actions.size() == 6);

  CHECK(config.actions[0].terminal_command == R"(notify-send -t 2000 -a "keymapper" "F7")");
  // in strings only $0... are substituted
  CHECK(config.actions[1].terminal_command == R"(echo $(echo $0$1))");
  CHECK(config.actions[2].terminal_command == R"(echo  echo )");
  CHECK(config.actions[3].terminal_command == R"(echo echo, echo)");
  CHECK(config.actions[4].terminal_command == R"(echo echo echo )");
  // in terminal commands macros are also substituted
  CHECK(config.actions[5].terminal_command == R"($(echo $0$1) "echo")");
}

//--------------------------------------------------------------------

TEST_CASE("Terminal command", "[ParseConfig]") {
  auto strings = {
    "A >>$(ls -la ; echo | cat)",
    R"(
      A >> action
      action >> $(ls -la ; echo | cat)  # comment
    )",
    R"(
      A >> action
      [class='test']
      action >> $(ls -la ; echo | cat)  ; comment
    )",
  };

  for (const auto& string : strings) {
    auto config = Config{ };
    REQUIRE_NOTHROW(config = parse_config(string));
    REQUIRE(config.actions.size() == 1);
    REQUIRE(config.actions[0].terminal_command == "ls -la ; echo | cat");
  }

  CHECK_THROWS(parse_config("A >> $"));
  CHECK_THROWS(parse_config("A >> $(ls "));
  CHECK_THROWS(parse_config("A >> A{ $(ls) }"));
  CHECK_THROWS(parse_config("A >> (A $(ls) )"));
}

//--------------------------------------------------------------------

TEST_CASE("Logical keys", "[ParseConfig]") {
  auto string = R"(
    Ext = IntlBackslash | AltRight
    Ext{A} >> ArrowLeft
  )";
  auto config = parse_config(string);
  REQUIRE(config.contexts.size() == 1);
  REQUIRE(config.contexts[0].inputs.size() == 2);
  REQUIRE(config.contexts[0].outputs.size() == 1);
  CHECK(format_sequence(config.contexts[0].inputs[0].input) == "+IntlBackslash +A ~A ~IntlBackslash");
  CHECK(config.contexts[0].inputs[0].output_index == 0);
  CHECK(format_sequence(config.contexts[0].inputs[1].input) == "+AltRight +A ~A ~AltRight");
  CHECK(config.contexts[0].inputs[1].output_index == 0);

  string = R"(
    Ext = IntlBackslash | AltRight
    Alt = AltLeft
    Ext2 = Ext | Alt
    Ext2{A} >> ArrowLeft
  )";
  config = parse_config(string);
  REQUIRE(config.contexts.size() == 1);
  REQUIRE(config.contexts[0].inputs.size() == 3);
  REQUIRE(config.contexts[0].outputs.size() == 1);
  CHECK(format_sequence(config.contexts[0].inputs[0].input) == "+IntlBackslash +A ~A ~IntlBackslash");
  CHECK(format_sequence(config.contexts[0].inputs[1].input) == "+AltRight +A ~A ~AltRight");
  CHECK(format_sequence(config.contexts[0].inputs[2].input) == "+AltLeft +A ~A ~AltLeft");

  string = R"(
    Ext = IntlBackslash | AltRight | AltLeft
    Macro = A $(ls -la | grep xy) B
    Ext{A} >> Macro
  )";
  config = parse_config(string);
  REQUIRE(config.contexts.size() == 1);
  REQUIRE(config.contexts[0].inputs.size() == 3);
  REQUIRE(config.contexts[0].outputs.size() == 1);
  CHECK(format_sequence(config.contexts[0].inputs[0].input) == "+IntlBackslash +A ~A ~IntlBackslash");
  CHECK(format_sequence(config.contexts[0].inputs[1].input) == "+AltRight +A ~A ~AltRight");
  CHECK(format_sequence(config.contexts[0].inputs[2].input) == "+AltLeft +A ~A ~AltLeft");
  CHECK(format_sequence(config.contexts[0].outputs[0]) == "+A -A +Action0 +B -B");
  REQUIRE(config.actions.size() == 1);
  CHECK(config.actions[0].terminal_command == "ls -la | grep xy");

  CHECK_THROWS(parse_config("Ext = A | "));
  CHECK_THROWS(parse_config("Ext = A | B |"));
  CHECK_THROWS(parse_config("Ext = A | something"));
  CHECK_THROWS(parse_config("A >> B | C"));
  CHECK_THROWS(parse_config("A | B >> C"));
}

//--------------------------------------------------------------------

TEST_CASE("Logical keys 2", "[ParseConfig]") {
  auto string = R"(
    Shift{A} >> Shift{B}
  )";

  auto config = parse_config(string);
  REQUIRE(config.contexts.size() == 1);
  REQUIRE(config.contexts[0].inputs.size() == 2);
  REQUIRE(config.contexts[0].outputs.size() == 2);
  REQUIRE(format_sequence(config.contexts[0].inputs[0].input) == "+ShiftLeft +A ~A ~ShiftLeft");
  REQUIRE(format_sequence(config.contexts[0].inputs[1].input) == "+ShiftRight +A ~A ~ShiftRight");
  REQUIRE(format_sequence(config.contexts[0].outputs[0]) == "+ShiftLeft +B -B -ShiftLeft");
  REQUIRE(format_sequence(config.contexts[0].outputs[1]) == "+ShiftRight +B -B -ShiftRight");
}

TEST_CASE("Logical keys in context filter", "[ParseConfig]") {
  auto string = R"(
    Mod = A | B | C
    
    [modifier = "Mod"] # 2 fallthrough contexts
    R >> X
    
    [modifier = "!Mod"]
    S >> Y
    
    [default]
    T >> Z
  )";

  auto config = parse_config(string);
  REQUIRE(config.contexts.size() == 5);
  REQUIRE(config.contexts[0].fallthrough);
  REQUIRE(config.contexts[0].inputs.empty());
  REQUIRE(config.contexts[1].fallthrough);
  REQUIRE(config.contexts[1].inputs.empty());
  REQUIRE(!config.contexts[2].fallthrough);
  REQUIRE(config.contexts[2].inputs.size() == 1);
  REQUIRE(!config.contexts[3].fallthrough);
  REQUIRE(config.contexts[3].inputs.size() == 1);
  REQUIRE(!config.contexts[4].fallthrough);
  REQUIRE(config.contexts[4].inputs.size() == 1);
  REQUIRE(format_sequence(config.contexts[0].modifier_filter) == "+A");
  REQUIRE(format_sequence(config.contexts[1].modifier_filter) == "+B");
  REQUIRE(format_sequence(config.contexts[2].modifier_filter) == "+C");
  REQUIRE(format_sequence(config.contexts[3].modifier_filter) == "!A !B !C");
  REQUIRE(format_sequence(config.contexts[4].modifier_filter) == "");
}

//--------------------------------------------------------------------

TEST_CASE("String escape sequence", "[ParseConfig]") {
  auto string = R"(
    A >> '\nnt\t'
    B >> $(echo "a\nb")
  )";

  auto config = parse_config(string);
  REQUIRE(config.contexts.size() == 1);
  REQUIRE(config.contexts[0].outputs.size() == 2);
  CHECK(format_sequence(config.contexts[0].outputs[0]) == 
    "!MetaLeft !MetaRight !ShiftLeft !ShiftRight !AltLeft !AltRight !ControlLeft !ControlRight "
    "+Enter -Enter +N -N +T -T +Tab -Tab");

  REQUIRE(config.actions.size() == 1);
  CHECK(config.actions[0].terminal_command == R"(echo "a\nb")");
}

//--------------------------------------------------------------------

TEST_CASE("Complex terminal commands", "[ParseConfig]") {
  auto string = R"(
    F1 >> $(i3-open -c Telegram "gtk-launch $(basename $(rg -l Telegram $HOME/.local/share/applications)) ")
  )";
  auto config = parse_config(string);
  REQUIRE(config.actions.size() == 1);
}
