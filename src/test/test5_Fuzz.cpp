
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
      REQUIRE(stage.is_clear());
  }
}

//--------------------------------------------------------------------

TEST_CASE("Fuzz no might match", "[Fuzz]") {
  const auto device_index = 0;
  auto config = R"(
    ? 1 2 3 >> A
    ? 2 3 4 >> B
    ? 4 5 6 >> C
    ? 7 8 >> D
    ? 9 8 7 >> E
    ? 7 6 5 4 >> F
  )";
  Stage stage = create_stage(config);

  auto keys = std::vector<Key>();
  for (auto k : { "1", "2", "3", "4", "5", "6", "7", "8", "9" })
    keys.push_back(parse_input(k).front().key);
  auto pressed = std::set<Key>();

  auto all_released = 0;
  auto rand = std::mt19937(0);
  auto dist = std::uniform_int_distribution<size_t>(0, keys.size() - 1);
  for (auto i = 0; i < 1000; i++) {

    // release all keys regularly
    if (pressed.empty() || ++all_released > 4) {
      for (auto key : pressed)
        stage.update({ key, KeyState::Up }, device_index);
      pressed.clear();
      all_released = 0;
    }

    const auto key = keys[dist(rand)];
    if (auto it = pressed.find(key); it != end(pressed)) {
      pressed.erase(it);
      stage.update({ key, KeyState::Up }, device_index);
    }
    else {
      pressed.insert(key);
      stage.update({ key, KeyState::Down }, device_index);
    }
    REQUIRE(stage.history_size() < 8);
  }
}
