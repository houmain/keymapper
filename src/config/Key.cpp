
#include "Key.h"
#include <cctype>
#include <string>
#include <algorithm>
#include <vector>

const char* get_key_name(const Key& key) {
  switch (key) {
    case Key::Esc: return "Esc";
    case Key::_1: return "1";
    case Key::_2: return "2";
    case Key::_3: return "3";
    case Key::_4: return "4";
    case Key::_5: return "5";
    case Key::_6: return "6";
    case Key::_7: return "7";
    case Key::_8: return "8";
    case Key::_9: return "9";
    case Key::_0: return "0";
    case Key::Minus: return "Minus";
    case Key::Equal: return "Equal";
    case Key::Backspace: return "Backspace";
    case Key::Tab: return "Tab";
    case Key::Q: return "Q";
    case Key::W: return "W";
    case Key::E: return "E";
    case Key::R: return "R";
    case Key::T: return "T";
    case Key::Y: return "Y";
    case Key::U: return "U";
    case Key::I: return "I";
    case Key::O: return "O";
    case Key::P: return "P";
    case Key::LeftBrace: return "LeftBrace";
    case Key::RightBrace: return "RightBrace";
    case Key::Enter: return "Enter";
    case Key::LeftCtrl: return "LeftCtrl";
    case Key::A: return "A";
    case Key::S: return "S";
    case Key::D: return "D";
    case Key::F: return "F";
    case Key::G: return "G";
    case Key::H: return "H";
    case Key::J: return "J";
    case Key::K: return "K";
    case Key::L: return "L";
    case Key::Semicolon: return "Semicolon";
    case Key::Apostrophe: return "Apostrophe";
    case Key::Grave: return "Grave";
    case Key::LeftShift: return "LeftShift";
    case Key::Backslash: return "Backslash";
    case Key::Z: return "Z";
    case Key::X: return "X";
    case Key::C: return "C";
    case Key::V: return "V";
    case Key::B: return "B";
    case Key::N: return "N";
    case Key::M: return "M";
    case Key::Comma: return "Comma";
    case Key::Dot: return "Dot";
    case Key::Slash: return "Slash";
    case Key::RightShift: return "RightShift";
    case Key::KPAsterisk: return "KPAsterisk";
    case Key::LeftAlt: return "LeftAlt";
    case Key::Space: return "Space";
    case Key::CapsLock: return "CapsLock";
    case Key::F1: return "F1";
    case Key::F2: return "F2";
    case Key::F3: return "F3";
    case Key::F4: return "F4";
    case Key::F5: return "F5";
    case Key::F6: return "F6";
    case Key::F7: return "F7";
    case Key::F8: return "F8";
    case Key::F9: return "F9";
    case Key::F10: return "F10";
    case Key::NumLock: return "NumLock";
    case Key::ScrollLock: return "ScrollLock";
    case Key::KP7: return "KP7";
    case Key::KP8: return "KP8";
    case Key::KP9: return "KP9";
    case Key::KPMinus: return "KPMinus";
    case Key::KP4: return "KP4";
    case Key::KP5: return "KP5";
    case Key::KP6: return "KP6";
    case Key::KPPlus: return "KPPlus";
    case Key::KP1: return "KP1";
    case Key::KP2: return "KP2";
    case Key::KP3: return "KP3";
    case Key::KP0: return "KP0";
    case Key::KPDot: return "KPDot";
    case Key::_102nd: return "102nd";
    case Key::F11: return "F11";
    case Key::F12: return "F12";
    case Key::KPEnter: return "KPEnter";
    case Key::RightCtrl: return "RightCtrl";
    case Key::KPSlash: return "KPSlash";
    case Key::SysRQ: return "SysRQ";
    case Key::RightAlt: return "RightAlt";
    case Key::Home: return "Home";
    case Key::Up: return "Up";
    case Key::PageUp: return "PageUp";
    case Key::Left: return "Left";
    case Key::Right: return "Right";
    case Key::End: return "End";
    case Key::Down: return "Down";
    case Key::PageDown: return "PageDown";
    case Key::Insert: return "Insert";
    case Key::Delete: return "Delete";
    case Key::Mute: return "Mute";
    case Key::VolumeDown: return "VolumeDown";
    case Key::VolumeUp: return "VolumeUp";
    case Key::KPEqual: return "KPEqual";
    case Key::Pause: return "Pause";
    case Key::KPComma: return "KPComma";
    case Key::LeftMeta: return "LeftMeta";
    case Key::RightMeta: return "RightMeta";
    case Key::Compose: return "Compose";
    case Key::WWW: return "WWW";
    case Key::Mail: return "Mail";
    case Key::Back: return "Back";
    case Key::Forward: return "Forward";
    case Key::NextSong: return "NextSong";
    case Key::PlayPause: return "PlayPause";
    case Key::PreviousSong: return "PreviousSong";
    case Key::BrightnessDown: return "BrightnessDown";
    case Key::BrightnessUp: return "BrightnessUp";

    case Key::Any: return "Any";
    case Key::Shift: return "Shift";
    case Key::Ctrl: return "Ctrl";
    case Key::Meta: return "Meta";

    case Key::Virtual1: return "Virtual1";
    case Key::Virtual2: return "Virtual2";
    case Key::Virtual3: return "Virtual3";
    case Key::Virtual4: return "Virtual4";
    case Key::Virtual5: return "Virtual5";
    case Key::Virtual6: return "Virtual6";
    case Key::Virtual7: return "Virtual7";
    case Key::Virtual8: return "Virtual8";

    case Key::None:
    case Key::Count:
      break;
  }
  return "???";
}

Key get_key_by_name(const std::string& name) {
  // generate vector of all key name/key pairs, sorted by name
  static const auto s_key_map =
    []() {
      const auto count = static_cast<int>(Key::Count);
      auto map = std::vector<std::pair<std::string, Key>>();
      map.reserve(count);

      for (auto k = 1; k < count; k++) {
        const auto key = static_cast<Key>(k);
        map.emplace_back(get_key_name(key), key);
      }
      std::sort(begin(map), end(map),
        [](const auto& a, const auto& b) { return a.first < b.first; });

      return map;
    }();

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
