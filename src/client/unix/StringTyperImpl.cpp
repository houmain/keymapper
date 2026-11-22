
#include "StringTyperImpl.h"
#include "common/output.h"
#include <algorithm>
#include <codecvt>
#include <locale>

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

namespace {
#if defined(__linux__)
  void hex_type_unicode_linux(uint32_t character,
    const std::map<char32_t, StringTyperImpl::Entry>& dictionary,
    const StringTyper::AddKey& add_key) {
    constexpr Key fallback_keys[] = {
      Key::Digit0, Key::Digit1, Key::Digit2, Key::Digit3,
      Key::Digit4, Key::Digit5, Key::Digit6, Key::Digit7,
      Key::Digit8, Key::Digit9, Key::A, Key::B, Key::C,
      Key::D, Key::E, Key::F,
    };
    constexpr char32_t hex_chars[] = U"0123456789abcdef";
    auto digits = std::vector<unsigned>{};
    for (; character > 0; character >>= 4)
      digits.push_back(character & 0xF);

    // Ctrl-Shift-U Hex-code Space
    add_key(Key::U, StringTyper::Modifier::Control | StringTyper::Modifier::Shift, 0);
    for (auto it = digits.rbegin(); it != digits.rend(); ++it) {
      const auto mapping = dictionary.find(hex_chars[*it]);
      if (mapping != dictionary.end()) {
        for (const auto& [key, modifiers] : mapping->second)
          add_key(key, modifiers, 0);
      } else {
        add_key(fallback_keys[*it], {}, 0);
      }
    }
    add_key(Key::Space, {}, 0);
  }

#endif // __linux__
} // namespace

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

//-------------------------------------------------------------------------

void StringTyperImpl::type(std::string_view string, const AddKey& add_key) const {
  auto characters = utf8_to_utf32(string);
  replace_all<char32_t>(characters, U"\\n", U"\r");
  replace_all<char32_t>(characters, U"\\r", U"\r");
  replace_all<char32_t>(characters, U"\\t", U"\t");

  for (auto character : characters) {
    auto it = m_dictionary.find(character);
    if (it == m_dictionary.end()) {
#if defined(__linux__)
      hex_type_unicode_linux(character, m_dictionary, add_key);
      continue;
#else
      it = m_dictionary.find('?');
#endif
    }

    if (it != m_dictionary.end())
      for (auto [key, modifiers] : it->second)
        add_key(key, modifiers, 0);
  }
}

//-------------------------------------------------------------------------

StringTyperImpl::KeyModifier StringTyperImpl::s_compose_key;

void set_string_typer_compose_key(Key key, StringTyper::Modifier modifier) {
  StringTyperImpl::s_compose_key = { key, modifier };
}

StringTyper::StringTyper() {
  const auto systems = std::initializer_list<std::pair<const char*, MakeStringTyperImpl*>>{
#if defined(ENABLE_WAYLAND) && defined(ENABLE_XKBCOMMON)
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
      verbose("Initialized string typing with %s layout (%d symbols)",
        name, m_impl->symbol_count());
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
