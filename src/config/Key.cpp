
#include "Key.h"
#include <cctype>
#include <cstring>
#include <string>
#include <algorithm>
#include <vector>

std::string_view get_key_name(const Key& key) {
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
    case Key::AudioVolumeMute:    return "AudioVolumeMute";
    case Key::AudioVolumeDown:    return "AudioVolumeDown";
    case Key::AudioVolumeUp:      return "AudioVolumeUp";
    case Key::Power:              return "Power";
    case Key::NumpadEqual:        return "NumpadEqual";
    case Key::Pause:              return "Pause";
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

    case Key::Any:      return "Any";
    case Key::Shift:    return "Shift";
    case Key::Control:  return "Control";
    case Key::Meta:     return "Meta";

    case Key::Virtual1: return "Virtual1";
    case Key::Virtual2: return "Virtual2";
    case Key::Virtual3: return "Virtual3";
    case Key::Virtual4: return "Virtual4";
    case Key::Virtual5: return "Virtual5";
    case Key::Virtual6: return "Virtual6";
    case Key::Virtual7: return "Virtual7";
    case Key::Virtual8: return "Virtual8";

    case Key::None:
      break;
  }
  return { };
}

Key get_key_by_name(std::string_view name) {
  // generate vector of all key name/key pairs, sorted by name
  static const auto s_key_map =
    []() {
      auto map = std::vector<std::pair<std::string_view, Key>>();
      map.reserve(200);

      for (auto key_code = 1; key_code < 0xFFFF; ++key_code) {
        const auto key = static_cast<Key>(key_code);
        auto name = get_key_name(key);
        if (!name.empty())
          map.emplace_back(name, key);
      }
      std::sort(begin(map), end(map),
        [](const auto& a, const auto& b) { return a.first < b.first; });

      return map;
    }();

  // allow to omit Key and Digit prefixes
  if (name.size() > 3 && name.substr(0, 3) == "Key")
    name = name.substr(3);
  else if (name.size() > 5 && name.substr(0, 5) == "Digit")
    name = name.substr(5);

  // binary search for key name
  auto it = std::lower_bound(cbegin(s_key_map), cend(s_key_map), name,
    [](const auto& kv, const auto& name) { return kv.first < name; });
  if (it != cend(s_key_map) && it->first == name)
    return it->second;

  return Key::None;
}

KeyCode operator*(Key key) {
  return static_cast<KeyCode>(key);
}
