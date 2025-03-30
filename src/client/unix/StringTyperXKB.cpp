
#include "StringTyperXKB.h"

#if defined(ENABLE_XKBCOMMON)
# include <xkbcommon/xkbcommon.h>
# include <xkbcommon/xkbcommon-compose.h>
#endif

constexpr uint32_t to_code(const char* name) {
  auto code = uint32_t{ };
  for (auto c = name; *c; ++c)
    code = ((code << 8) | *c);
  return code;
}

Key xkb_keyname_to_key(const char* name) {
  if (!name || !name[0])
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

const char* get_locale() {
  if (auto locale = getenv("LC_ALL"))
    return locale;
  if (auto locale = getenv("LC_CTYPE"))
    return locale;
  if (auto locale = getenv("LANG"))
    return locale;
  return "C";
}

//-------------------------------------------------------------------------

bool StringTyperXKB::update_layout_xkbcommon(
  xkb_context* context, xkb_keymap* keymap) {

#if defined(ENABLE_XKBCOMMON)
  if (!context || !keymap)
    return false;
  const auto min = xkb_keymap_min_keycode(keymap);
  const auto max = xkb_keymap_max_keycode(keymap);
  auto symbols = std::add_pointer_t<const xkb_keysym_t>{ };
  auto masks = std::array<xkb_mod_mask_t, 8>{ };

  auto dead_keys = std::map<xkb_keysym_t, Entry>();
  auto stroke_by_keysymbol = std::map<xkb_keysym_t, const Entry*>();
  if (s_compose_key.key != Key::none) {
    const auto it = dead_keys.emplace(XKB_KEY_Multi_key, Entry{ s_compose_key }).first;
    stroke_by_keysymbol[XKB_KEY_Multi_key] = &it->second;
  }

  m_dictionary.clear();
  for (auto keycode = min; keycode < max; ++keycode)
    if (auto name = xkb_keymap_key_get_name(keymap, keycode))
      if (auto key = xkb_keyname_to_key(name); key != Key::none) {
        const auto layouts = xkb_keymap_num_layouts_for_key(keymap, keycode);
        for (auto layout = 0u; layout < layouts; ++layout) {
          const auto levels = xkb_keymap_num_levels_for_key(keymap, keycode, layout);
          for (auto level = 0u; level < levels; ++level) {
            const auto num_symbols = xkb_keymap_key_get_syms_by_level(keymap, keycode,
              layout, level, &symbols);
            const auto num_masks = xkb_keymap_key_get_mods_for_level(keymap, keycode,
              layout, level, masks.data(), masks.size());
            if (num_symbols > 0 && num_masks > 0) {
              const auto symbol = symbols[0];
              const auto mask = masks[0];
              if (auto character = xkb_keysym_to_utf32(symbol)) {
                if (m_dictionary.find(character) == m_dictionary.end()) {
                  const auto it = m_dictionary.emplace(character,
                    Entry{ { key, get_xkb_modifiers(mask) } }).first;
                  stroke_by_keysymbol[symbol] = &it->second;
                }
              }
              else {
                const auto it = dead_keys.emplace(symbol,
                  Entry{ { key, get_xkb_modifiers(mask) } }).first;
                stroke_by_keysymbol[symbol] = &it->second;
              }
            }
          }
        }
      }

#if defined(ENABLE_XKBCOMMON_COMPOSE)
  const auto compose_entry = [&](xkb_compose_table_entry* entry) {
    auto result = Entry{ };
    auto length = size_t{ };
    const auto sequence = xkb_compose_table_entry_sequence(entry, &length);
    for (auto i = 0u; i < length; ++i) {
      const auto it = stroke_by_keysymbol.find(sequence[i]);
      if (it == stroke_by_keysymbol.end())
        return Entry{ };
      result.push_back(it->second->front());
    }
    return result;
  };

  if (s_compose_key.key != Key::none) {
    const auto locale = get_locale();
    const auto compose_table = xkb_compose_table_new_from_locale(
      context, locale, XKB_COMPOSE_COMPILE_NO_FLAGS);
    if (compose_table) {
      auto it = xkb_compose_table_iterator_new(compose_table);
      while (const auto entry = xkb_compose_table_iterator_next(it))
        if (auto entry_utf8 = xkb_compose_table_entry_utf8(entry); *entry_utf8) {
          const auto character = utf8_to_utf32(entry_utf8)[0];
          if (m_dictionary.find(character) == m_dictionary.end())
            if (auto composed = compose_entry(entry); !composed.empty())
              m_dictionary[character] = std::move(composed);
      }
      xkb_compose_table_iterator_free(it);
      xkb_compose_table_unref(compose_table);
    }
  }
#endif // ENABLE_XKBCOMMON_COMPOSE
  return true;
#else // !ENABLE_XKBCOMMON
  return false;
#endif
}
