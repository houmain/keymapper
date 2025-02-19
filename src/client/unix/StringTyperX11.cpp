
#if defined(ENABLE_X11)

#include "StringTyperXKB.h"
#include <X11/Xlib.h>
#include <X11/XKBlib.h>

#if defined(ENABLE_XKBCOMMON)
# include <X11/Xlib-xcb.h>
# include <xkbcommon/xkbcommon.h>
# include <xkbcommon/xkbcommon-x11.h>
#endif

class StringTyperX11 : public StringTyperXKB {
public:
  bool update_layout() {
    auto display = XOpenDisplay(nullptr);
    if (!display)
      return false;

    auto result = update_layout_xcb_xkbcommon(display);
    if (!result)
      result = update_layout_x11(display);

    XCloseDisplay(display);
    return result;
  }

private:
  bool update_layout_x11(Display* display) {
    auto xim = XOpenIM(display, 0, 0, 0);
    auto xic = XCreateIC(xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, nullptr);
    auto kb_desc = XkbGetMap(display, 0, XkbUseCoreKbd);
    XkbGetNames(display, XkbKeyNamesMask, kb_desc);

    auto utf8_char = std::array<char, 32>{ };

    m_dictionary.clear();
    for_each_modifier_combination(std::array{ ShiftMask, Mod1Mask, Mod5Mask }, [&](auto state) {    
      for (auto keycode = int{ kb_desc->min_key_code }; keycode <= kb_desc->max_key_code; ++keycode)
        if (const auto key = xkb_keyname_to_key(kb_desc->names->keys[keycode].name); key != Key::none) {
          auto event = XKeyPressedEvent{ };
          event.type = KeyPress;
          event.display = display;
          event.keycode = keycode;
          event.state = state;
          auto keysym = KeySym{ };
          auto status = Status{ };
          const auto len = Xutf8LookupString(xic, &event,
            utf8_char.data(), utf8_char.size(), &keysym, &status);
          if (status == XLookupChars || status == XLookupBoth) {
            const auto character = utf8_to_utf32(std::string_view(utf8_char.data(), len))[0];
            const auto it = m_dictionary.find(character);
            if (it == m_dictionary.end())
              m_dictionary[character] = { key, get_xkb_modifiers(state) };
          }
        }
    });

    XkbFreeNames(kb_desc, 0, True);
    XDestroyIC(xic);
    XCloseIM(xim);
    return true;
  }

  bool update_layout_xcb_xkbcommon(Display* display) {
#if defined(ENABLE_XKBCOMMON)
    auto xcb_connection = XGetXCBConnection(display);
    if (!xcb_connection)
      return false;

    auto xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    auto device_id = xkb_x11_get_core_keyboard_device_id(xcb_connection);
    auto xkb_keymap = xkb_x11_keymap_new_from_device(xkb_context,
      xcb_connection, device_id, XKB_KEYMAP_COMPILE_NO_FLAGS);

    auto result = update_layout_xkbcommon(xkb_context, xkb_keymap);

    xkb_keymap_unref(xkb_keymap);
    xkb_context_unref(xkb_context);
    return result;
#else // !ENABLE_XKBCOMMON
    return false;
#endif
  }
};

std::unique_ptr<StringTyperImpl> make_string_typer_x11() {
  auto impl = std::make_unique<StringTyperX11>();
  if (!impl->update_layout())
    return { };
  return impl;
}

#endif // ENABLE_X11
