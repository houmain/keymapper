
#include "StringTyperImpl.h"
#include "common/output.h"
#include <codecvt>
#include <locale>

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

using MakeStringTyperImpl = std::unique_ptr<StringTyperImpl>();
MakeStringTyperImpl make_string_typer_wayland;
MakeStringTyperImpl make_string_typer_x11;
MakeStringTyperImpl make_string_typer_carbon;
MakeStringTyperImpl make_string_typer_generic;

std::u32string utf8_to_utf32(std::string_view utf8_string) {
  auto utf8to32 = std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t>();
  return utf8to32.from_bytes(utf8_string.begin(), utf8_string.end());
}

std::string utf16_to_utf8(std::u16string_view utf16_string) {
  auto utf8to16 = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>();
  return utf8to16.to_bytes(utf16_string.begin(), utf16_string.end());
}

std::u32string utf16_to_utf32(std::u16string_view utf16_string) {
  return utf8_to_utf32(utf16_to_utf8(utf16_string));
}

constexpr uint32_t to_code(const char* name) {
  return (name[0] << 24) | (name[1] << 16) | (name[2] << 8) | (name[3] << 0);
}

Key xkb_keyname_to_key(const char* name) {
  if (!name[0])
    return Key::none;

  const auto name_code = to_code(name);
  switch (name_code) {
    case to_code("ESC"): return Key::Escape;
    case to_code("AE01"): return Key::Digit1;
    case to_code("AE02"): return Key::Digit2;
    case to_code("AE03"): return Key::Digit3;
    case to_code("AE04"): return Key::Digit4;
    case to_code("AE05"): return Key::Digit5;
    case to_code("AE06"): return Key::Digit6;
    case to_code("AE07"): return Key::Digit7;
    case to_code("AE08"): return Key::Digit8;
    case to_code("AE09"): return Key::Digit9;
    case to_code("AE10"): return Key::Digit0;
    case to_code("AE11"): return Key::Minus;
    case to_code("AE12"): return Key::Equal;
    case to_code("BKSP"): return Key::Backspace;
    case to_code("TAB"):  return Key::Tab;
    case to_code("AD01"): return Key::Q;
    case to_code("AD02"): return Key::W;
    case to_code("AD03"): return Key::E;
    case to_code("AD04"): return Key::R;
    case to_code("AD05"): return Key::T;
    case to_code("AD06"): return Key::Y;
    case to_code("AD07"): return Key::U;
    case to_code("AD08"): return Key::I;
    case to_code("AD09"): return Key::O;
    case to_code("AD10"): return Key::P;
    case to_code("AD11"): return Key::BracketLeft;
    case to_code("AD12"): return Key::BracketRight;
    case to_code("AC01"): return Key::A;
    case to_code("AC02"): return Key::S;
    case to_code("AC03"): return Key::D;
    case to_code("AC04"): return Key::F;
    case to_code("AC05"): return Key::G;
    case to_code("AC06"): return Key::H;
    case to_code("AC07"): return Key::J;
    case to_code("AC08"): return Key::K;
    case to_code("AC09"): return Key::L;
    case to_code("AC10"): return Key::Semicolon;
    case to_code("AC11"): return Key::Quote;
    case to_code("AC12"): return Key::IntlRo; // untested
    case to_code("TLDE"): return Key::Backquote;
    case to_code("BKSL"): return Key::Backslash;
    case to_code("AB01"): return Key::Z;
    case to_code("AB02"): return Key::X;
    case to_code("AB03"): return Key::C;
    case to_code("AB04"): return Key::V;
    case to_code("AB05"): return Key::B;
    case to_code("AB06"): return Key::N;
    case to_code("AB07"): return Key::M;
    case to_code("AB08"): return Key::Comma;
    case to_code("AB09"): return Key::Period;
    case to_code("AB10"): return Key::Slash;
    case to_code("SPCE"): return Key::Space;
    case to_code("LSGT"): return Key::IntlBackslash;
    case to_code("RTRN"): return Key::Enter;
  }
  return Key::none;
}

StringTyper::Modifiers get_xkb_modifiers(uint32_t mask) {
  const auto ShiftMask = (1 << 0);
  const auto Mod1Mask  = (1 << 3);
  const auto Mod5Mask  = (1 << 7);
  auto modifiers = StringTyper::Modifiers{ };
  if (mask & ShiftMask) modifiers |= StringTyper::Shift;
  if (mask & Mod1Mask)  modifiers |= StringTyper::Alt;
  if (mask & Mod5Mask)  modifiers |= StringTyper::AltGr;
  return modifiers;
}

//-------------------------------------------------------------------------

void StringTyperImpl::type(std::string_view string, const AddKey& add_key) const {
  auto characters = utf8_to_utf32(string);
  replace_all<char32_t>(characters, U"\\n", U"\r");
  replace_all<char32_t>(characters, U"\\r", U"\r");
  replace_all<char32_t>(characters, U"\\t", U"\t");

  for (auto character : characters)
    if (auto it = m_dictionary.find(character); it != m_dictionary.end())
      add_key(it->second.key, it->second.modifiers);
}

//-------------------------------------------------------------------------

StringTyper::StringTyper() {
  const auto systems = std::initializer_list<std::pair<const char*, MakeStringTyperImpl*>>{
#if defined(ENABLE_WAYLAND)
    { "Wayland", &make_string_typer_wayland },
#endif
#if defined(ENABLE_CARBON)
    { "Carbon", &make_string_typer_carbon },
#endif
#if defined(ENABLE_X11)
    { "X11", &make_string_typer_x11 },
#endif
    { "generic", &make_string_typer_generic },
  };

  for (auto [name, make_system] : systems)
    if (auto system = make_system()) {
      m_impl = std::move(system);
      verbose("Initialized string typing with %s layout", name);
      break;
    }
}

StringTyper::StringTyper(StringTyper&& rhs) noexcept = default;
StringTyper& StringTyper::operator=(StringTyper&& rhs) noexcept = default;
StringTyper::~StringTyper() = default;

void StringTyper::type(std::string_view string, const AddKey& add_key) const {
  if (m_impl)
    m_impl->type(string, add_key);
}
