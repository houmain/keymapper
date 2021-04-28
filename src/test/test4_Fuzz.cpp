
#include "test.h"
#include "config/ParseConfig.h"
#include "runtime/Stage.h"
#include <set>
#include <random>

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
} // namespace

//--------------------------------------------------------------------

TEST_CASE("Fuzz #1", "[Fuzz]") {
  auto config = R"(
    Ext = IntlBackslash
    Ext          >>
    Ext{W{K}}    >> 1
    ShiftLeft{L} >> !ShiftLeft 2
    J            >> 3 ^ 4
    Ext{W{Any}}  >>
  )";
  Stage stage = create_stage(config);

  auto keys = std::vector<KeyCode>();
  for (auto k : { "IntlBackslash", "ShiftLeft", "W", "K", "L", "J", "I" })
    keys.push_back(parse_input(k).front().key);
  auto pressed = std::set<KeyCode>();

  auto rand = std::mt19937(0);
  auto dist = std::uniform_int_distribution<size_t>(0, keys.size() - 1);
  for (auto i = 0; i < 1000; i++) {
    const auto key = keys[dist(rand)];
    if (auto it = pressed.find(key); it != end(pressed)) {
      pressed.erase(it);
      stage.apply_input({ key, KeyState::Up });
    }
    else {
      pressed.insert(key);
      stage.apply_input({ key, KeyState::Down });
    }
    if (pressed.empty())
      CHECK(!stage.is_output_down());
  }
}

//--------------------------------------------------------------------
