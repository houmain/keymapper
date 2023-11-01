
#if defined(ENABLE_X11)

#include "StringTyperImpl.h"
#include "runtime/Key.h"
#include "config/StringTyper.h"
#include <map>
#include <array>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>

namespace {
  constexpr uint32_t to_code(const char* name) {
    return (name[0] << 24) | (name[1] << 16) | (name[2] << 8) | (name[3] << 0);
  }

  Key xkb_keycode_to_key(const char* name) {
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
      case to_code("AC12"): return Key::Backquote;
      case to_code("TLDE"): return Key::IntlRo;
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
    }
    return Key::none;
  }

  StringTyper::Modifiers get_modifiers(int state) {
    auto modifiers = StringTyper::Modifiers{ };
    if (state & ShiftMask) modifiers |= StringTyper::Shift;
    if (state & Mod1Mask)  modifiers |= StringTyper::Alt;
    if (state & Mod5Mask)  modifiers |= StringTyper::AltGr;
    return modifiers;
  }

  // string_views which start with string are regarded as equal
  struct starts_with_less {
    using is_transparent = std::true_type;

    bool operator()(const std::string& a, const std::string& b) const {
      return (a < b);
    }

    bool operator()(const std::string& a, const std::string_view& b) const {
      if (b.starts_with(a))
        return false;
      return (a < b);
    }

    bool operator()(const std::string_view& a, const std::string& b) const {
      if (a.starts_with(b))
        return false;
      return (a < b);
    }
  };
} // namespace

class StringTyperX11 : public StringTyperImpl {
private:
  using Character = std::string;

  struct Entry {
    Key key;
    int state;
  };
  std::map<Character, Entry, starts_with_less> m_dictionary;

public:
  bool update_layout() {
    auto display = XOpenDisplay(nullptr);
    if (!display)
      return false;
    auto xim = XOpenIM(display, 0, 0, 0);
    auto xic = XCreateIC(xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, nullptr);
    auto kb_desc = XkbGetMap(display, 0, XkbUseCoreKbd);
    XkbGetNames(display, XkbKeyNamesMask, kb_desc);

    const auto state_masks = std::array{ ShiftMask, Mod1Mask, Mod5Mask };
    auto utf8_char = std::array<char, 32>{ };

    m_dictionary.clear();
    for (auto keycode = int{ kb_desc->min_key_code }; keycode <= kb_desc->max_key_code; ++keycode)
      if (const auto key = xkb_keycode_to_key(kb_desc->names->keys[keycode].name); key != Key::none)
        for (auto s = 0x00; s < (1 << state_masks.size()); ++s) {

          auto state = 0x00;
          for (auto i = 0; i < state_masks.size(); ++i)
            if (s & (1 << i))
              state |= state_masks[i];

          auto event = XKeyPressedEvent{ };
          event.type = KeyPress;
          event.display = display;
          event.keycode = keycode;
          event.state = state;
          auto keysym = KeySym{ };
          auto status = Status{ };
          const auto len = Xutf8LookupString(xic, &event,
            utf8_char.data(), utf8_char.size(), &keysym, &status);
          if (len > 0 && static_cast<unsigned char>(utf8_char[0]) > 31) {
            const auto it = m_dictionary.find(std::string_view(utf8_char.data(), len));
            if (it == m_dictionary.end() || it->second.state > state)
              m_dictionary[utf8_char.data()] = { key, state };
          }
        }

    XkbFreeNames(kb_desc, 0, True);
    XDestroyIC(xic);
    XCloseIM(xim);
    XCloseDisplay(display);
    return true;
  }

  void type(std::string_view string, const AddKey& add_key) const override {
    for (auto i = 0; i < string.size(); ) {
      if (auto it = m_dictionary.find(string.substr(i)); it != m_dictionary.end()) {
        add_key(it->second.key, get_modifiers(it->second.state));
        i += it->first.size();
      }
      else {
        ++i;
      }
    }
  }
};

std::unique_ptr<StringTyperImpl> make_string_typer_x11() {
  auto impl = std::make_unique<StringTyperX11>();
  if (!impl->update_layout())
    return { };
  return impl;
}

#endif // ENABLE_X11
