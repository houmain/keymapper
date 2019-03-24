
#include "key_scan_code.h"

unsigned int get_scan_code(Key key) {
  auto extended = true;
  switch (key) {
    case Key::RIGHTSHIFT: break;
    case Key::NUMLOCK: break;
    case Key::LEFTMETA: key = Key::HIRAGANA; break;
    case Key::RIGHTMETA: key = Key::KATAKANAHIRAGANA; break;
    case Key::PRINT: key = Key::KPASTERISK; break;
    case Key::DELETE: key = Key::KPDOT; break;
    case Key::INSERT: key = Key::KP0; break;
    case Key::END: key = Key::KP1; break;
    case Key::DOWN: key = Key::KP2; break;
    case Key::PAGEDOWN: key = Key::KP3; break;
    case Key::LEFT: key = Key::KP4; break;
    case Key::RIGHT: key = Key::KP6; break;
    case Key::HOME: key = Key::KP7; break;
    case Key::UP: key = Key::KP8; break;
    case Key::PAGEUP: key = Key::KP9; break;
    case Key::RIGHTCTRL: key = Key::LEFTCTRL; break;
    case Key::RIGHTALT: key = Key::LEFTALT; break;
    case Key::KPSLASH: key = Key::SLASH; break;
    case Key::KPENTER: key = Key::ENTER; break;
    case Key::PAUSE:
      key = Key::NUMLOCK;
      extended = false;
      break;
    default:
      extended = false;
      break;
  }
  return static_cast<unsigned int>(key) | (extended ? 0xE000 : 0);
}

Key get_key(unsigned int scan_code) {
  if (!(scan_code & 0xFF) || (scan_code & 0x0F00))
    return Key::NONE;

  const auto extended = (scan_code & 0xE000 ? true : false);
  auto key = static_cast<Key>(scan_code & ~0xE000u);
  if (extended) {
    switch (key) {
      case Key::RIGHTSHIFT: break;
      case Key::NUMLOCK: break;
      case Key::HIRAGANA: key = Key::LEFTMETA; break;
      case Key::KATAKANAHIRAGANA: key = Key::RIGHTMETA; break;
      case Key::KPASTERISK: key = Key::PRINT; break;
      case Key::KPDOT: key = Key::DELETE; break;
      case Key::KP0: key = Key::INSERT; break;
      case Key::KP1: key = Key::END; break;
      case Key::KP2: key = Key::DOWN; break;
      case Key::KP3: key = Key::PAGEDOWN; break;
      case Key::KP4: key = Key::LEFT; break;
      case Key::KP6: key = Key::RIGHT; break;
      case Key::KP7: key = Key::HOME; break;
      case Key::KP8: key = Key::UP; break;
      case Key::KP9: key = Key::PAGEUP; break;
      case Key::LEFTCTRL: key = Key::RIGHTCTRL; break;
      case Key::LEFTALT: key = Key::RIGHTALT; break;
      case Key::SLASH: key = Key::KPSLASH; break;
      case Key::ENTER: key = Key::KPENTER; break;
      default:
        break;
    }
  }
  else {
    switch (key) {
      case Key::NUMLOCK: key = Key::PAUSE; break;
      default:
        break;
    }
  }
  return key;
}
