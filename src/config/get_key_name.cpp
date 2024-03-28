
#include "get_key_name.h"
#include <cctype>
#include <cstring>
#include <string>
#include <algorithm>
#include <vector>
#include <sstream>

namespace {
  Key try_parse_key_code(std::string_view name) {
    if (name.size() >= 2 && (name[0] >= '0' && name[0] <= '9')) {
      auto rest = char{ };
      auto keycode = size_t{ };
      auto ss = std::istringstream(std::string(name));
      ss.unsetf(std::ios_base::basefield);
      ss >> keycode >> rest;
      if (ss.eof() && keycode > 0 && rest == 0 && keycode > 0 && keycode < 0xF000)
        return static_cast<Key>(keycode);
    }
    return Key::none;
  }
} // namespace

const char* get_key_name(const Key& key) {
  switch (key) {
    case Key::Escape:             return "Escape";
    case Key::Digit1:             return "1";
    case Key::Digit2:             return "2";
    case Key::Digit3:             return "3";
    case Key::Digit4:             return "4";
    case Key::Digit5:             return "5";
    case Key::Digit6:             return "6";
    case Key::Digit7:             return "7";
    case Key::Digit8:             return "8";
    case Key::Digit9:             return "9";
    case Key::Digit0:             return "0";
    case Key::Minus:              return "Minus";
    case Key::Equal:              return "Equal";
    case Key::Backspace:          return "Backspace";
    case Key::Tab:                return "Tab";
    case Key::KeyQ:               return "Q";
    case Key::KeyW:               return "W";
    case Key::KeyE:               return "E";
    case Key::KeyR:               return "R";
    case Key::KeyT:               return "T";
    case Key::KeyY:               return "Y";
    case Key::KeyU:               return "U";
    case Key::KeyI:               return "I";
    case Key::KeyO:               return "O";
    case Key::KeyP:               return "P";
    case Key::BracketLeft:        return "BracketLeft";
    case Key::BracketRight:       return "BracketRight";
    case Key::Enter:              return "Enter";
    case Key::ControlLeft:        return "ControlLeft";
    case Key::KeyA:               return "A";
    case Key::KeyS:               return "S";
    case Key::KeyD:               return "D";
    case Key::KeyF:               return "F";
    case Key::KeyG:               return "G";
    case Key::KeyH:               return "H";
    case Key::KeyJ:               return "J";
    case Key::KeyK:               return "K";
    case Key::KeyL:               return "L";
    case Key::Semicolon:          return "Semicolon";
    case Key::Quote:              return "Quote";
    case Key::Backquote:          return "Backquote";
    case Key::ShiftLeft:          return "ShiftLeft";
    case Key::Backslash:          return "Backslash";
    case Key::KeyZ:               return "Z";
    case Key::KeyX:               return "X";
    case Key::KeyC:               return "C";
    case Key::KeyV:               return "V";
    case Key::KeyB:               return "B";
    case Key::KeyN:               return "N";
    case Key::KeyM:               return "M";
    case Key::Comma:              return "Comma";
    case Key::Period:             return "Period";
    case Key::Slash:              return "Slash";
    case Key::ShiftRight:         return "ShiftRight";
    case Key::NumpadMultiply:     return "NumpadMultiply";
    case Key::AltLeft:            return "AltLeft";
    case Key::Space:              return "Space";
    case Key::CapsLock:           return "CapsLock";
    case Key::F1:                 return "F1";
    case Key::F2:                 return "F2";
    case Key::F3:                 return "F3";
    case Key::F4:                 return "F4";
    case Key::F5:                 return "F5";
    case Key::F6:                 return "F6";
    case Key::F7:                 return "F7";
    case Key::F8:                 return "F8";
    case Key::F9:                 return "F9";
    case Key::F10:                return "F10";
    case Key::NumLock:            return "NumLock";
    case Key::ScrollLock:         return "ScrollLock";
    case Key::Numpad7:            return "Numpad7";
    case Key::Numpad8:            return "Numpad8";
    case Key::Numpad9:            return "Numpad9";
    case Key::NumpadSubtract:     return "NumpadSubtract";
    case Key::Numpad4:            return "Numpad4";
    case Key::Numpad5:            return "Numpad5";
    case Key::Numpad6:            return "Numpad6";
    case Key::NumpadAdd:          return "NumpadAdd";
    case Key::Numpad1:            return "Numpad1";
    case Key::Numpad2:            return "Numpad2";
    case Key::Numpad3:            return "Numpad3";
    case Key::Numpad0:            return "Numpad0";
    case Key::NumpadDecimal:      return "NumpadDecimal";
    case Key::IntlBackslash:      return "IntlBackslash";
    case Key::F11:                return "F11";
    case Key::F12:                return "F12";
    case Key::IntlRo:             return "IntlRo";
    case Key::Convert:            return "Convert";
    case Key::KanaMode:           return "KanaMode";
    case Key::NonConvert:         return "NonConvert";
    case Key::NumpadEnter:        return "NumpadEnter";
    case Key::ControlRight:       return "ControlRight";
    case Key::NumpadDivide:       return "NumpadDivide";
    case Key::PrintScreen:        return "PrintScreen";
    case Key::AltRight:           return "AltRight";
    case Key::Home:               return "Home";
    case Key::ArrowUp:            return "ArrowUp";
    case Key::PageUp:             return "PageUp";
    case Key::ArrowLeft:          return "ArrowLeft";
    case Key::ArrowRight:         return "ArrowRight";
    case Key::End:                return "End";
    case Key::ArrowDown:          return "ArrowDown";
    case Key::PageDown:           return "PageDown";
    case Key::Insert:             return "Insert";
    case Key::Delete:             return "Delete";
    case Key::Settings:           return "Settings";
    case Key::BrightnessDown:     return "BrightnessDown";
    case Key::BrightnessUp:       return "BrightnessUp";
    case Key::DisplayToggleIntExt:return "DisplayToggleIntExt";
    case Key::Prog3:              return "Prog3";
    case Key::WLAN:               return "WLAN";
    case Key::LaunchApp2:         return "LaunchApp2";
    case Key::AudioVolumeMute:    return "AudioVolumeMute";
    case Key::AudioVolumeDown:    return "AudioVolumeDown";
    case Key::AudioVolumeUp:      return "AudioVolumeUp";
    case Key::Power:              return "Power";
    case Key::NumpadEqual:        return "NumpadEqual";
    case Key::Pause:              return "Pause";
    case Key::Cancel:             return "Cancel";
    case Key::NumpadComma:        return "NumpadComma";
    case Key::Lang1:              return "Lang1";
    case Key::Lang2:              return "Lang2";
    case Key::IntlYen:            return "IntlYen";
    case Key::MetaLeft:           return "MetaLeft";
    case Key::MetaRight:          return "MetaRight";
    case Key::ContextMenu:        return "ContextMenu";
    case Key::BrowserStop:        return "BrowserStop";
    case Key::LaunchApp1:         return "LaunchApp1";
    case Key::BrowserSearch:      return "BrowserSearch";
    case Key::BrowserFavorites:   return "BrowserFavorites";
    case Key::BrowserBack:        return "BrowserBack";
    case Key::BrowserForward:     return "BrowserForward";
    case Key::MediaTrackNext:     return "MediaTrackNext";
    case Key::MediaPlayPause:     return "MediaPlayPause";
    case Key::MediaTrackPrevious: return "MediaTrackPrevious";
    case Key::MediaStop:          return "MediaStop";
    case Key::BrowserRefresh:     return "BrowserRefresh";
    case Key::BrowserHome:        return "BrowserHome";
    case Key::LaunchMail:         return "LaunchMail";
    case Key::LaunchMediaPlayer:  return "LaunchMediaPlayer";
    case Key::Again:              return "Again";
    case Key::Props:              return "Props";
    case Key::Undo:               return "Undo";
    case Key::Select:             return "Select";
    case Key::Copy:               return "Copy";
    case Key::Open:               return "Open";
    case Key::Paste:              return "Paste";
    case Key::Find:               return "Find";
    case Key::Cut:                return "Cut";
    case Key::Help:               return "Help";
    case Key::Sleep:              return "Sleep";
    case Key::WakeUp:             return "WakeUp";
    case Key::Eject:              return "Eject";
    case Key::Fn:                 return "Fn";
    case Key::F13:                return "F13";
    case Key::F14:                return "F14";
    case Key::F15:                return "F15";
    case Key::F16:                return "F16";
    case Key::F17:                return "F17";
    case Key::F18:                return "F18";
    case Key::F19:                return "F19";
    case Key::F20:                return "F20";
    case Key::F21:                return "F21";
    case Key::F22:                return "F22";
    case Key::F23:                return "F23";
    case Key::F24:                return "F24";

    case Key::ButtonLeft:         return "ButtonLeft";
    case Key::ButtonRight:        return "ButtonRight";
    case Key::ButtonMiddle:       return "ButtonMiddle";
    case Key::ButtonBack:         return "ButtonBack";
    case Key::ButtonForward:      return "ButtonForward";

    case Key::any:                return "Any";
    case Key::ContextActive:      return "ContextActive";

    case Key::none:
    case Key::timeout:
    case Key::last_keyboard_key:
    case Key::first_virtual:
    case Key::last_virtual:
    case Key::first_logical:
    case Key::last_logical:
    case Key::first_action:
    case Key::last_action:
    case Key::Control:
    case Key::Meta:
      break;
  }
  return nullptr;
}

template<size_t SizeZ>
bool remove_prefix(std::string_view& name, const char(&literal)[SizeZ]) {
  const auto length = SizeZ - 1;
  if (name.size() <= length || name.substr(0, length) != literal)
    return false;
  name = name.substr(length);
  return true;
}

Key get_key_by_name(std::string_view name) {
  if (name == "Any")
    return Key::any;

  if (auto key = try_parse_key_code(name); key != Key::none)
    return key;

  if (remove_prefix(name, "Virtual"))
    if (const auto n = std::atoi(name.data()); n >= 0)
      if (n <= *Key::last_virtual - *Key::first_virtual)
        return static_cast<Key>(*Key::first_virtual + n);

  // allow to omit Key and Digit prefixes
  if (!remove_prefix(name, "Key"))
    remove_prefix(name, "Digit");

  // generate vector of all key name/key pairs, sorted by name
  static const auto s_key_map =
    []() {
      auto map = std::vector<std::pair<std::string_view, Key>>();
      map.reserve(200);

      for (auto key_code = 1; key_code < 0xFFFF; ++key_code) {
        const auto key = static_cast<Key>(key_code);
        if (auto name = get_key_name(key))
          map.emplace_back(name, key);
      }
      std::sort(begin(map), end(map),
        [](const auto& a, const auto& b) { return a.first < b.first; });

      return map;
    }();

  // binary search for key name
  const auto it = std::lower_bound(cbegin(s_key_map), cend(s_key_map), name,
    [](const auto& kv, const auto& name) { return kv.first < name; });
  if (it != cend(s_key_map) && it->first == name)
    return it->second;

  // allow some aliases
  if (name == "OSLeft")
    return Key::MetaLeft;
  if (name == "OSRight")
    return Key::MetaRight;

  return Key::none;
}

