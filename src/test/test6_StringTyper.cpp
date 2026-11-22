#if defined(__linux__)
#include "test.h"
#include "client/unix/StringTyperImpl.h"

namespace {
  class TestStringTyper : public StringTyperImpl {
  public:
    void map(char32_t character, Key key, Modifiers modifiers = {}) {
      m_dictionary[character] = { { key, modifiers } };
    }
  };

  using Stroke = std::pair<Key, StringTyper::Modifiers>;
  std::vector<Stroke> strokes(const TestStringTyper& typer, const char* input) {
    auto result = std::vector<Stroke>{};
    typer.type(input, [&](Key key, int modifiers, uint16_t) {
      result.emplace_back(key, modifiers);
    });
    return result;
  }
}

TEST_CASE("Linux unicode entry does not rely on Num Lock", "[StringTyper]") {
  const auto typer = TestStringTyper{};
  const auto start = Stroke{ Key::U, StringTyper::Control | StringTyper::Shift };
  const auto end = Stroke{ Key::Space, 0 };
  CHECK(strokes(typer, "ë") == std::vector<Stroke>{start, {Key::E, 0}, {Key::B, 0}, end});
  CHECK(strokes(typer, "ê") == std::vector<Stroke>{start, {Key::E, 0}, {Key::A, 0}, end});
  CHECK(strokes(typer, "è") == std::vector<Stroke>{start, {Key::E, 0}, {Key::Digit8, 0}, end});
  CHECK(strokes(typer, "é") == std::vector<Stroke>{start, {Key::E, 0}, {Key::Digit9, 0}, end});
}

TEST_CASE("Linux unicode entry uses active layout for hex digits", "[StringTyper]") {
  auto typer = TestStringTyper{};
  typer.map(U'e', Key::Q, StringTyper::AltGr);
  typer.map(U'8', Key::Digit8, StringTyper::Shift);
  const auto expected = std::vector<Stroke>{
    {Key::U, StringTyper::Control | StringTyper::Shift},
    {Key::Q, StringTyper::AltGr}, {Key::Digit8, StringTyper::Shift}, {Key::Space, 0}
  };
  CHECK(strokes(typer, "è") == expected);

  // A direct layout mapping still takes precedence over Unicode entry.
  typer.map(U'è', Key::X, StringTyper::Shift);
  CHECK(strokes(typer, "è") == std::vector<Stroke>{ {Key::X, StringTyper::Shift} });
}
#endif
