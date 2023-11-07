
#if defined(ENABLE_X11)

#include "StringTyperImpl.h"
#include "runtime/Key.h"
#include "config/StringTyper.h"
#include <map>
#include <array>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>

class StringTyperX11 : public StringTyperImpl {
private:
  struct Entry {
    Key key;
    int state;
  };
  std::map<char32_t, Entry> m_dictionary;

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
      if (const auto key = xkb_keyname_to_key(kb_desc->names->keys[keycode].name); key != Key::none)
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
            const auto character = utf8_to_utf32(std::string_view(utf8_char.data(), len))[0];
            const auto it = m_dictionary.find(character);
            if (it == m_dictionary.end() || it->second.state > state)
              m_dictionary[character] = { key, state };
          }
        }

    XkbFreeNames(kb_desc, 0, True);
    XDestroyIC(xic);
    XCloseIM(xim);
    XCloseDisplay(display);
    return true;
  }

  void type(std::string_view string, const AddKey& add_key) const override {
    for (auto character : utf8_to_utf32(string))
      if (auto it = m_dictionary.find(character); it != m_dictionary.end())
        add_key(it->second.key, get_xkb_modifiers(it->second.state));
  }
};

std::unique_ptr<StringTyperImpl> make_string_typer_x11() {
  auto impl = std::make_unique<StringTyperX11>();
  if (!impl->update_layout())
    return { };
  return impl;
}

#endif // ENABLE_X11
