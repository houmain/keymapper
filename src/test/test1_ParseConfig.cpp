
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
    MyMacro = A B C# comment "

    Shift{A} >> B
    C >> CommandA ; comment $(
    CommandA >> X
    E >> CommandB

    ; comment
    [ system = "Windows" class='test'title=test ] # comment
    CommandA >> Y        #; comment /
    CommandB >> MyMacro   ;# comment '

    [system='Linux', title=/firefox[123]*x{1,3}/i ] # comment
    CommandA >> Shift{Y}      ## comment
    CommandB >> Shift{MyMacro}  ;; comment
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
    [class='abc']
    CommandB >> D
  )";
  CHECK_THROWS(parse_config(string));

  // duplicate mapping of command
  string = R"(
    C >> CommandA

    [class='abc']
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

    [class='abc']
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

  // empty class
  string = R"(
    C >> CommandA
    [class='']
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

TEST_CASE("Config directives", "[ParseConfig]") {
  CHECK_THROWS(parse_config(R"(@unknown)"));
  CHECK_NOTHROW(parse_config(R"(@allow-unmapped-commands)"));
  CHECK_NOTHROW(parse_config(R"(@allow-unmapped-commands true)"));
  CHECK_NOTHROW(parse_config(R"(@allow-unmapped-commands false)"));
  CHECK_THROWS(parse_config(R"(@allow-unmapped-commands True)"));
  CHECK_THROWS(parse_config(R"(@allow-unmapped-commands true a)"));

  CHECK_THROWS(parse_config(R"(
    A >> command
  )"));

  CHECK_THROWS(parse_config(R"(
    command >> A
  )"));

  CHECK_NOTHROW(parse_config(R"(
    @allow-unmapped-commands
    A >> command
  )"));

  CHECK_NOTHROW(parse_config(R"(
    @allow-unmapped-commands
    A >> Command   # assumes that Command is a command
  )"));

  CHECK_NOTHROW(parse_config(R"(
    @allow-unmapped-commands
    command >> A
  )"));

  CHECK_THROWS(parse_config(R"(
    @allow-unmapped-commands
    Command >> A   # assumes that Command is a key
  )"));

  CHECK_NOTHROW(parse_config(R"(
    A >> Command
    Command >> B
  )"));

  CHECK_THROWS(parse_config(R"(
    @enforce-lowercase-commands
    A >> Command
    Command >> B
  )"));

  CHECK_NOTHROW(parse_config(R"(
    @enforce-lowercase-commands
    A >> command
    command >> B

    # ensure numbers are not treated as lowercase
    31 >> 64
  )"));

  CHECK_THROWS(parse_config(R"(
    @allow-unmapped-commands
    @enforce-lowercase-commands
    A >> Command
  )"));

  CHECK_NOTHROW(parse_config(R"(
    @allow-unmapped-commands
    @enforce-lowercase-commands
    A >> command
  )"));

  CHECK_NOTHROW(parse_config(R"(
    @allow-unmapped-commands
    @enforce-lowercase-commands
    command >> A
  )"));

  CHECK_THROWS(parse_config(R"(
    @allow-unmapped-commands
    @enforce-lowercase-commands
    command >> UndefinedKey   # output is still validated
  )"));

  CHECK_NOTHROW(parse_config(R"(
    @enforce-lowercase-commands true
    A >> command
    @enforce-lowercase-commands false
    B >> Command
    @allow-unmapped-commands
  )"));
}

//--------------------------------------------------------------------

TEST_CASE("Forward modifiers directive", "[ParseConfig]") {
  CHECK_NOTHROW(parse_config(R"(
    @forward-modifiers
    @forward-modifiers A ; comment
    @forward-modifiers Shift Control # comment
  )"));
  CHECK_THROWS(parse_config(R"(@forward-modifiers Shft)"));
  CHECK_THROWS(parse_config(R"(@forward-modifiers Shift ^)"));
  CHECK_THROWS(parse_config(R"(@forward-modifiers 100ms)"));
  CHECK_THROWS(parse_config(R"(@forward-modifiers Any)"));
  CHECK_THROWS(parse_config(R"(@forward-modifiers ContextActive)"));

  auto config = parse_config(R"(
    @forward-modifiers Shift Control Shift AltLeft ControlLeft # duplicates are removed
    Control{A} >> Shift{X}
    Control{B} >> command
    ControlLeft{C} >> command
    AltLeft{D} >> command
    command >> Shift{Y}

    [title="app"]
    command >> Control{Z}

    [stage]
    ShiftRight{X} >> Z
  )");
  REQUIRE(config.contexts.size() == 3);
  const auto& c0 = config.contexts[0];
  const auto& c1 = config.contexts[1];
  const auto& c2 = config.contexts[2];
  REQUIRE(c0.inputs.size() == 11);
  REQUIRE(c0.outputs.size() == 6);
  REQUIRE(c0.command_outputs.size() == 1);
  // inputs are inserted at the front
  CHECK(format_sequence(c0.inputs[0].input) == "+ShiftLeft ~ShiftLeft");
  CHECK(format_sequence(c0.inputs[1].input) == "+ShiftRight ~ShiftRight");
  CHECK(format_sequence(c0.inputs[2].input) == "+ControlLeft ~ControlLeft");
  CHECK(format_sequence(c0.inputs[3].input) == "+ControlRight ~ControlRight");
  CHECK(format_sequence(c0.inputs[4].input) == "+AltLeft ~AltLeft");
  CHECK(format_sequence(c0.inputs[5].input) == "+ControlLeft +A ~A ~ControlLeft");
  CHECK(format_sequence(c0.inputs[6].input) == "+ControlRight +A ~A ~ControlRight");
  CHECK(format_sequence(c0.inputs[7].input) == "+ControlLeft +B ~B ~ControlLeft");
  CHECK(format_sequence(c0.inputs[8].input) == "+ControlRight +B ~B ~ControlRight");
  CHECK(format_sequence(c0.inputs[9].input) == "+ControlLeft +C ~C ~ControlLeft");
  CHECK(format_sequence(c0.inputs[10].input) == "+AltLeft +D ~D ~AltLeft");
  // forwarded modifiers in input are suppressed in output
  CHECK(format_sequence(c0.command_outputs[0].output) == 
    "!AltLeft !ControlRight !ControlLeft +ShiftLeft +Y -Y -ShiftLeft");
  CHECK(format_sequence(c0.outputs[0]) == 
    "!ControlRight !ControlLeft +ShiftLeft +X -X -ShiftLeft");
  // outputs are prepended (in reverse order)
  CHECK(format_sequence(c0.outputs[1]) == "+AltLeft");
  CHECK(format_sequence(c0.outputs[2]) == "+ControlRight");
  CHECK(format_sequence(c0.outputs[3]) == "+ControlLeft");
  CHECK(format_sequence(c0.outputs[4]) == "+ShiftRight");
  CHECK(format_sequence(c0.outputs[5]) == "+ShiftLeft");

  REQUIRE(c1.inputs.size() == 0);
  REQUIRE(c1.outputs.size() == 0);
  REQUIRE(c1.command_outputs.size() == 1);
  CHECK(format_sequence(c1.command_outputs[0].output) == 
    "!AltLeft !ControlRight +ControlLeft +Z -Z -ControlLeft");

  // modifiers are forwarded in each stage
  REQUIRE(c2.inputs.size() == 6);
  REQUIRE(c2.outputs.size() == 6);
  REQUIRE(c2.command_outputs.empty());
  CHECK(format_sequence(c2.inputs[0].input) == "+ShiftLeft ~ShiftLeft");
  CHECK(format_sequence(c2.inputs[1].input) == "+ShiftRight ~ShiftRight");
  CHECK(format_sequence(c2.inputs[2].input) == "+ControlLeft ~ControlLeft");
  CHECK(format_sequence(c2.inputs[3].input) == "+ControlRight ~ControlRight");
  CHECK(format_sequence(c2.inputs[4].input) == "+AltLeft ~AltLeft");
  CHECK(format_sequence(c2.inputs[5].input) == "+ShiftRight +X ~X ~ShiftRight");
  CHECK(format_sequence(c2.outputs[0]) == "!ShiftRight +Z");
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

TEST_CASE("Context filters #2", "[ParseConfig]") {
  auto string = R"(
    A >> command

    [device_id = "usb-Dell_Dell_Multimedia_Pro_Keyboard-event-kbd"]
    command >> B
  )";

  auto config = parse_config(string);
  REQUIRE(config.contexts.size() == 2);
  CHECK(config.contexts[1].device_id_filter.string == "usb-Dell_Dell_Multimedia_Pro_Keyboard-event-kbd");
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
  CHECK(config.contexts[0].device_filter.string == "/Device1/");
  CHECK(config.contexts[0].window_class_filter.string == "A");
  CHECK(config.contexts[1].device_filter.string == "DeviceB");
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
  CHECK(config.contexts[0].device_filter.invert);
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

TEST_CASE("Stages", "[ParseConfig]") {
  auto string = R"(
    A >> B    # 0

    [stage]   # 1, no fallthrough
    [default] # 2
    B >> C
    
    [default] # removed
    [stage]   # 3
    C >> D

    [default] # removed

    [stage]   # 4
    E >> F

    [default] # removed

    [default] # 5
    F >> G

    [stage]   # 6, no fallthrough
    [stage]   # 7

    [default] # 8
    H >> J
  )";

  auto config = parse_config(string);
  REQUIRE(config.contexts.size() == 9);
  CHECK(config.contexts[0].begin_stage);
  CHECK(config.contexts[1].begin_stage);
  CHECK(!config.contexts[2].begin_stage);
  CHECK(config.contexts[3].begin_stage);
  CHECK(config.contexts[4].begin_stage);
  CHECK(!config.contexts[5].begin_stage);
  CHECK(config.contexts[6].begin_stage);
  CHECK(config.contexts[7].begin_stage);
  CHECK(!config.contexts[8].begin_stage);
  for (const auto& context : config.contexts)
    CHECK(!context.fallthrough);
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
    "+E -E +F -F !Any +A -A +B -B +E -E +F -F");

  // allow to override key names
  string = R"(
    Alt{A} >> X

    Alt = AltRight
    Alt{B} >> Y
  )";
  REQUIRE_NOTHROW(config = parse_config(string));
  REQUIRE(config.contexts[0].inputs.size() == 3);
  REQUIRE(config.contexts[0].outputs.size() == 2);
  REQUIRE(config.contexts[0].command_outputs.size() == 0);
  CHECK(format_sequence(config.contexts[0].inputs[0].input) == "+AltLeft +A ~A ~AltLeft");
  CHECK(format_sequence(config.contexts[0].inputs[1].input) == "+AltRight +A ~A ~AltRight");
  CHECK(format_sequence(config.contexts[0].inputs[2].input) == "+AltRight +B ~B ~AltRight");
  CHECK(format_sequence(config.contexts[0].outputs[0]) == "+X");
  CHECK(format_sequence(config.contexts[0].outputs[1]) == "+Y");
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

  CHECK(format_sequence(config.contexts[0].outputs[1]) == "!Any +ShiftLeft +X -X -ShiftLeft");

  CHECK(format_sequence(config.contexts[0].outputs[2]) == "!Any +A -A +Space -Space +B -B");

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

TEST_CASE("Macros with arguments #2", "[ParseConfig]") {
  auto string = R"(
    LOG_FILE = "logfile.txt"
    base00       = "#657b83"
    logColor     = $(pastel --force-color paint "$1" "[$0]" >> "${LOG_FILE}")
    stage1       = 50ms logColor["$0", blue] ^ logColor["$0", base00]
    F1 >> stage1["EditMode"]
  )";
  auto config = parse_config(string);
  REQUIRE(config.contexts.size() == 1);
  REQUIRE(config.actions.size() == 2);
  CHECK(config.actions[0].terminal_command == R"(pastel --force-color paint "blue" "[EditMode]" >> "logfile.txt")");
  CHECK(config.actions[1].terminal_command == R"(pastel --force-color paint "#657b83" "[EditMode]" >> "logfile.txt")");
}
//--------------------------------------------------------------------

TEST_CASE("Macro with string arguments", "[ParseConfig]") {
  auto string = R"(
    test = "$0"
    A >> test["test"]
  )";
  auto config = parse_config(string);
  REQUIRE(config.contexts.size() == 1);
  REQUIRE(config.contexts[0].outputs.size() == 1);
  CHECK(format_sequence(config.contexts[0].outputs[0]) == "!Any +T -T +E -E +S -S +T -T");
}

//--------------------------------------------------------------------

TEST_CASE("Macros with Alias arguments", "[ParseConfig]") {
  auto string = R"(
    twice = $0 $0
    Boss = Virtual1
    make = $0$1
    A >> twice[X]
    B >> twice[Boss]
    C >> make[make[Arr, ow], make[Le, ft]]
  )";
  auto config = parse_config(string);
  REQUIRE(config.contexts.size() == 1);
  REQUIRE(config.contexts[0].outputs.size() == 3);
  CHECK(format_sequence(config.contexts[0].outputs[0]) == "+X -X +X -X");
  CHECK(format_sequence(config.contexts[0].outputs[1]) == "+Virtual1 -Virtual1 +Virtual1 -Virtual1");
  CHECK(format_sequence(config.contexts[0].outputs[2]) == "+ArrowLeft");
}

//--------------------------------------------------------------------

TEST_CASE("Macros with Terminal Commands", "[ParseConfig]") {
  auto string = R"(
    notify = $(notify-send -t 2000 -a "keymapper" "$0")
    F1 >> notify["F7"]

    echo = $(echo $0$1)
    F2 >> echo[echo]
    F3 >> echo["echo$echo"]
    F4 >> echo["echo, echo"]
    F5 >> echo["echo", " echo "]
    F6 >> $(echo ${echo})
  )";
  auto config = parse_config(string);
  REQUIRE(config.actions.size() == 6);

  CHECK(config.actions[0].terminal_command == R"(notify-send -t 2000 -a "keymapper" "F7")");
  // in strings and terminal commands only $0... are substituted
  CHECK(config.actions[1].terminal_command == R"(echo $(echo $0$1))");
  CHECK(config.actions[2].terminal_command == R"(echo echo$(echo $0$1))");
  CHECK(config.actions[3].terminal_command == R"(echo echo, echo)");
  CHECK(config.actions[4].terminal_command == R"(echo echo echo )");
  CHECK(config.actions[5].terminal_command == R"(echo $(echo $0$1))");
}

//--------------------------------------------------------------------

TEST_CASE("Macro result substituted again", "[ParseConfig]") {
  auto string = R"(
    x0 = 
    x1 = x0[$0] $0
    x2 = x1[$0] $0

    xi = x$0
    x = xi[$0][$1]
    xZ = x[$0, Z]

    A >> xi[0][X]
    B >> x[1, Y]
    C >> xZ[2]
    D >> xi[i][2][W]
  )";
  auto config = parse_config(string);
  REQUIRE(config.contexts.size() == 1);
  REQUIRE(config.contexts[0].outputs.size() == 4);
  CHECK(format_sequence(config.contexts[0].outputs[0]) == "");
  CHECK(format_sequence(config.contexts[0].outputs[1]) == "+Y");
  CHECK(format_sequence(config.contexts[0].outputs[2]) == "+Z -Z +Z -Z");
  CHECK(format_sequence(config.contexts[0].outputs[3]) == "+W -W +W -W");
}

//--------------------------------------------------------------------

TEST_CASE("Builtin Macros", "[ParseConfig]") {
  auto string = R"(
    macro1 = repeat[$0, length["$1"]]
    macro2 = repeat[$0, $1]
    A >> repeat[A, length["Hello"]]
    B >> macro1[B, "Hello"]
    C >> macro2[C, length["Hello"]]
    D >> repeat[repeat[A, 2] B, 2]
  )";
  auto config = parse_config(string);
  REQUIRE(config.contexts.size() == 1);
  REQUIRE(config.contexts[0].outputs.size() == 4);
  CHECK(format_sequence(config.contexts[0].outputs[0]) == 
    "+A -A +A -A +A -A +A -A +A -A");
  CHECK(format_sequence(config.contexts[0].outputs[1]) == 
    "+B -B +B -B +B -B +B -B +B -B");
  CHECK(format_sequence(config.contexts[0].outputs[2]) == 
    "+C -C +C -C +C -C +C -C +C -C");
  CHECK(format_sequence(config.contexts[0].outputs[3]) == 
    "+A -A +A -A +B -B +A -A +A -A +B -B");

  CHECK_THROWS(parse_config("A >> length[A]"));
  CHECK_THROWS(parse_config("A >> repeat[A,]"));
  CHECK_THROWS(parse_config("A >> repeat[A, A]"));
}

//--------------------------------------------------------------------

TEST_CASE("Builtin Macros #2", "[ParseConfig]") {
  auto string = R"(
    type = $(keymapperctl --type $0)

    # Replace trigger string with the second argument
    trigger = ? "$0" >> repeat[Backspace, sub[length["$0"], 1]] $1

    # Format date
    format_date = $(date +"$0")

    trigger[":time", type[format_date["%H:%M:%S"]]]
  )";
  auto config = parse_config(string);
  REQUIRE(config.contexts.size() == 1);
  REQUIRE(config.actions.size() == 1);
  CHECK(config.actions[0].terminal_command == 
    R"(keymapperctl --type $(date +"%H:%M:%S"))");
}

//--------------------------------------------------------------------

TEST_CASE("Builtin Macros #3", "[ParseConfig]") {
  auto string = R"(
    test = default[$1, R] $0
    A >> test[X]
    B >> test[Y, S]
  )";
  auto config = parse_config(string);
  REQUIRE(config.contexts.size() == 1);
  REQUIRE(config.contexts[0].outputs.size() == 2);
  CHECK(format_sequence(config.contexts[0].outputs[0]) == "+R -R +X -X");
  CHECK(format_sequence(config.contexts[0].outputs[1]) == "+S -S +Y -Y");
}

//--------------------------------------------------------------------

TEST_CASE("Builtin Macros #4", "[ParseConfig]") {
  auto string = R"(
    log1 = $0 1
    log2 = $0 2 $1
    logN = log$0
    log = logN[$#][$0, $1]
    A >> log[X]
    B >> log[Y, S]
  )";
  auto config = parse_config(string);
  REQUIRE(config.contexts.size() == 1);
  REQUIRE(config.contexts[0].outputs.size() == 2);
  CHECK(format_sequence(config.contexts[0].outputs[0]) == "+X -X +1 -1");
  CHECK(format_sequence(config.contexts[0].outputs[1]) == "+Y -Y +2 -2 +S -S");
}

//--------------------------------------------------------------------

TEST_CASE("Top-level Macro", "[ParseConfig]") {
  auto string = R"(
    macro = A >> B ; comment
    shift = >> # comment
    context = [title = "$0"] # comment
    subst = ? "$0" >> repeat[Backspace, sub[length["$0"], 1]] "$1" # comment

    macro  # comment
    C shift D ; comment

    context["Test"] # comment
    subst["cat", "dog"] ; comment
  )";
  auto config = parse_config(string);
  REQUIRE(config.contexts.size() == 2);
  REQUIRE(config.contexts[0].inputs.size() == 2);
  REQUIRE(config.contexts[0].outputs.size() == 2);
  CHECK(format_sequence(config.contexts[0].inputs[0].input) == "+A ~A");
  CHECK(format_sequence(config.contexts[0].outputs[0]) == "+B");
  CHECK(format_sequence(config.contexts[0].inputs[1].input) == "+C ~C");
  CHECK(format_sequence(config.contexts[0].outputs[1]) == "+D");

  REQUIRE(config.contexts[1].inputs.size() == 1);
  REQUIRE(config.contexts[1].outputs.size() == 1);
  REQUIRE(config.contexts[1].window_title_filter.string == "Test");
  CHECK(format_sequence(config.contexts[1].inputs[0].input) == "? !ShiftLeft !ShiftRight !AltLeft !AltRight !ControlLeft !ControlRight +C ~C +A ~A +T");
  CHECK(format_sequence(config.contexts[1].outputs[0]) == "+Backspace -Backspace +Backspace -Backspace !Any +D -D +O -O +G -G");
  
  // scope
  CHECK_NOTHROW(config = parse_config(R"(
    macro = A
    macro = macro B
    macro = macro C
    macro >> macro
  )"));
  CHECK(format_sequence(config.contexts[0].outputs[0]) == 
    "+A -A +B -B +C -C");
  
  CHECK_THROWS(parse_config(R"(
    default = [default]
    default
  )"));
  
  CHECK_NOTHROW(parse_config(R"(
    Mouse = /Mouse|Logitech USB Receiver|G502/i
    [device = Mouse]
  )"));
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

TEST_CASE("Special characters in string and macro", "[ParseConfig]") {
  auto string = R"(
    trigger = $(pastel "#cb4b16" "[trigger = $0]")
    A >> trigger["["]
    B >> trigger["]"]
    C >> trigger["#"]
    D >> trigger[";"]
  )";

  auto config = parse_config(string);
  REQUIRE(config.actions.size() == 4);
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
    Ext = IntlBackslash | AltRight | AltLeft | AltRight  # duplicates are not removed (not ideal)
    Macro = A $(ls -la | grep xy) B
    Ext{A} >> Macro
    X >> !Ext Y
  )";
  config = parse_config(string);
  REQUIRE(config.contexts.size() == 1);
  REQUIRE(config.contexts[0].inputs.size() == 5);
  REQUIRE(config.contexts[0].outputs.size() == 2);
  CHECK(format_sequence(config.contexts[0].inputs[0].input) == "+IntlBackslash +A ~A ~IntlBackslash");
  CHECK(format_sequence(config.contexts[0].inputs[1].input) == "+AltRight +A ~A ~AltRight");
  CHECK(format_sequence(config.contexts[0].inputs[2].input) == "+AltLeft +A ~A ~AltLeft");
  CHECK(format_sequence(config.contexts[0].inputs[3].input) == "+AltRight +A ~A ~AltRight");
  CHECK(format_sequence(config.contexts[0].inputs[4].input) == "+X ~X");
  CHECK(format_sequence(config.contexts[0].outputs[0]) == "+A -A +Action0 +B -B");
  CHECK(format_sequence(config.contexts[0].outputs[1]) == "!IntlBackslash !AltRight !AltLeft !AltRight +Y");
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
    
    [modifier = "!Mod !Mod Mod"]  # duplicates are removed
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
    "!Any +Enter -Enter +N -N +T -T +Tab -Tab");

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

//--------------------------------------------------------------------

TEST_CASE("Device directives", "[ParseConfig]") {
  auto string = R"(
    @skip-device /.*/

    [system = "Linux"]
    @grab-device "MyKeyboard 1"  # comment

    [system = "Windows"]
    @grab-device "MyKeyboard 2"

    [system = "MacOS"]
    @grab-device "MyKeyboard 3"

    [default]
    @grab-device "MyKeyboard"
  )";
  auto config = parse_config(string);
  REQUIRE(config.grab_device_filters.size() == 3);
  CHECK(config.grab_device_filters[0].invert == true);
  CHECK(config.grab_device_filters[0].regex.has_value());
  CHECK(config.grab_device_filters[0].string == "/.*/");

  CHECK(config.grab_device_filters[1].invert == false);
  CHECK(!config.grab_device_filters[1].regex.has_value());

  CHECK(config.grab_device_filters[2].invert == false);
  CHECK(!config.grab_device_filters[2].regex.has_value());
  CHECK(config.grab_device_filters[2].string == "MyKeyboard");

  // Problems
  CHECK_THROWS(parse_config("@skip-dev /.*/"));
  CHECK_THROWS(parse_config("@skip-device /.*/ X"));
}

//--------------------------------------------------------------------

TEST_CASE("Line break", "[ParseConfig]") {
  auto string = R"(
    A >> \
    B \
    C 
  )";
  auto config = parse_config(string);
  REQUIRE(config.contexts.size() == 1);
  CHECK(format_sequence(config.contexts[0].inputs[0].input) == "+A ~A");
  CHECK(format_sequence(config.contexts[0].outputs[0]) == "+B -B +C -C");
}

//--------------------------------------------------------------------

TEST_CASE("String interpolation", "[ParseConfig]") {
  auto string = R"(
    TEST = "bc"
    A >> "${TEST}TEST$TEST";
    [title = /${TEST}TEST$TEST/]
    B >> $(${TEST}TEST$TEST)
    D >> $(${TEST1 TEST$TEST1)
  )";
  auto config = parse_config(string);
  REQUIRE(config.contexts.size() == 2);
  REQUIRE(config.contexts[0].outputs.size() == 1);
  REQUIRE(config.actions.size() == 2);
  CHECK(format_sequence(config.contexts[0].outputs[0]) == 
    "!Any +B -B +C -C +ShiftLeft +T -T +E -E +S -S +T -T -ShiftLeft +B -B +C -C");
  CHECK(config.contexts[1].window_title_filter.string == "/bcTESTbc/");
  CHECK(config.actions[0].terminal_command == R"(bcTESTbc)");
  CHECK(config.actions[1].terminal_command == R"(${TEST1 TEST$TEST1)");
}
