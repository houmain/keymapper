
#include "test.h"
#include <set>
#include <random>

TEST_CASE("Fuzz #1", "[Fuzz]") {
  const auto device_index = 0;
  auto config = R"(
    Ext = IntlBackslash
    Ext          >>
    Ext{W{K}}    >> 1
    ShiftLeft{L} >> !ShiftLeft 2
    J            >> 3 ^ 4
    Ext{W{Any}}  >>
  )";
  Stage stage = create_stage(config);

  auto keys = std::vector<Key>();
  for (auto k : { "IntlBackslash", "ShiftLeft", "W", "K", "L", "J", "I" })
    keys.push_back(parse_input(k).front().key);
  auto pressed = std::set<Key>();

  auto rand = std::mt19937(0);
  auto dist = std::uniform_int_distribution<size_t>(0, keys.size() - 1);
  for (auto i = 0; i < 1000; i++) {
    const auto key = keys[dist(rand)];
    if (auto it = pressed.find(key); it != end(pressed)) {
      pressed.erase(it);
      stage.update({ key, KeyState::Up }, device_index);
    }
    else {
      pressed.insert(key);
      stage.update({ key, KeyState::Down }, device_index);
    }
    if (pressed.empty())
      CHECK(stage.is_clear());
  }
}

//--------------------------------------------------------------------
