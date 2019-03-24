
#include "key_names.h"
#include <cctype>
#include <string>
#include <algorithm>
#include <vector>

namespace {
  std::string to_upper(std::string str) {
    for (auto& c : str)
      c = static_cast<char>(std::toupper(c));
    return str;
  }
} // namespace

const char* get_key_name(const Key& key) {
  switch (key) {
    case Key::ESC: return "Esc";
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
    case Key::MINUS: return "Minus";
    case Key::EQUAL: return "Equal";
    case Key::BACKSPACE: return "Backspace";
    case Key::TAB: return "Tab";
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
    case Key::LEFTBRACE: return "LeftBrace";
    case Key::RIGHTBRACE: return "RightBrace";
    case Key::ENTER: return "Enter";
    case Key::LEFTCTRL: return "LeftCtrl";
    case Key::A: return "A";
    case Key::S: return "S";
    case Key::D: return "D";
    case Key::F: return "F";
    case Key::G: return "G";
    case Key::H: return "H";
    case Key::J: return "J";
    case Key::K: return "K";
    case Key::L: return "L";
    case Key::SEMICOLON: return "Semicolon";
    case Key::APOSTROPHE: return "Apostrophe";
    case Key::GRAVE: return "Grave";
    case Key::LEFTSHIFT: return "LeftShift";
    case Key::BACKSLASH: return "Backslash";
    case Key::Z: return "Z";
    case Key::X: return "X";
    case Key::C: return "C";
    case Key::V: return "V";
    case Key::B: return "B";
    case Key::N: return "N";
    case Key::M: return "M";
    case Key::COMMA: return "Comma";
    case Key::DOT: return "Dot";
    case Key::SLASH: return "Slash";
    case Key::RIGHTSHIFT: return "RightShift";
    case Key::KPASTERISK: return "KPAsterisk";
    case Key::LEFTALT: return "LeftAlt";
    case Key::SPACE: return "Space";
    case Key::CAPSLOCK: return "Capslock";
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
    case Key::NUMLOCK: return "NumLock";
    case Key::SCROLLLOCK: return "ScrollLock";
    case Key::KP7: return "KP7";
    case Key::KP8: return "KP8";
    case Key::KP9: return "KP9";
    case Key::KPMINUS: return "KPMinus";
    case Key::KP4: return "KP4";
    case Key::KP5: return "KP5";
    case Key::KP6: return "KP6";
    case Key::KPPLUS: return "KPPlus";
    case Key::KP1: return "KP1";
    case Key::KP2: return "KP2";
    case Key::KP3: return "KP3";
    case Key::KP0: return "KP0";
    case Key::KPDOT: return "KPDot";
    case Key::ZENKAKUHANKAKU: return "ZENKAKUHANKAKU";
    case Key::_102ND: return "102ND";
    case Key::F11: return "F11";
    case Key::F12: return "F12";
    case Key::RO: return "RO";
    case Key::KATAKANA: return "KATAKANA";
    case Key::HIRAGANA: return "HIRAGANA";
    case Key::HENKAN: return "HENKAN";
    case Key::KATAKANAHIRAGANA: return "KATAKANAHIRAGANA";
    case Key::MUHENKAN: return "MUHENKAN";
    case Key::KPJPCOMMA: return "KPJPComma";
    case Key::KPENTER: return "KPEnter";
    case Key::RIGHTCTRL: return "RightCtrl";
    case Key::KPSLASH: return "KPSlash";
    case Key::SYSRQ: return "SYSRQ";
    case Key::RIGHTALT: return "RightAlt";
    case Key::LINEFEED: return "LineFeed";
    case Key::HOME: return "Home";
    case Key::UP: return "Up";
    case Key::PAGEUP: return "PageUp";
    case Key::LEFT: return "Left";
    case Key::RIGHT: return "Right";
    case Key::END: return "End";
    case Key::DOWN: return "Down";
    case Key::PAGEDOWN: return "PageDown";
    case Key::INSERT: return "Insert";
    case Key::DELETE: return "Delete";
    case Key::MACRO: return "Macro";
    case Key::MUTE: return "Mute";
    case Key::VOLUMEDOWN: return "VolumeDown";
    case Key::VOLUMEUP: return "VolumeUp";
    case Key::POWER: return "Power";
    case Key::KPEQUAL: return "KPEqual";
    case Key::KPPLUSMINUS: return "KPPlusMinus";
    case Key::PAUSE: return "Pause";
    case Key::SCALE: return "Scale";
    case Key::KPCOMMA: return "KPComma";
    case Key::HANGEUL: return "HANGEUL";
    case Key::HANJA: return "HANJA";
    case Key::YEN: return "Yen";
    case Key::LEFTMETA: return "LeftMeta";
    case Key::RIGHTMETA: return "RightMeta";
    case Key::COMPOSE: return "Compose";
    case Key::STOP: return "Stop";
    case Key::AGAIN: return "Again";
    case Key::PROPS: return "Props";
    case Key::UNDO: return "Undo";
    case Key::FRONT: return "Front";
    case Key::COPY: return "Copy";
    case Key::OPEN: return "Open";
    case Key::PASTE: return "Paste";
    case Key::FIND: return "Find";
    case Key::CUT: return "Cut";
    case Key::HELP: return "Help";
    case Key::MENU: return "Menu";
    case Key::CALC: return "Calc";
    case Key::SETUP: return "Setup";
    case Key::SLEEP: return "Sleep";
    case Key::WAKEUP: return "Wakeup";
    case Key::FILE: return "File";
    case Key::SENDFILE: return "SendFile";
    case Key::DELETEFILE: return "DeleteFile";
    case Key::XFER: return "XFER";
    case Key::PROG1: return "Prog1";
    case Key::PROG2: return "Prog2";
    case Key::WWW: return "WWW";
    case Key::MSDOS: return "MSDOS";
    case Key::SCREENLOCK: return "ScreenLock";
    case Key::DIRECTION: return "Direction";
    case Key::CYCLEWINDOWS: return "CycleWindows";
    case Key::MAIL: return "Mail";
    case Key::BOOKMARKS: return "Bookmarks";
    case Key::COMPUTER: return "Computer";
    case Key::BACK: return "Back";
    case Key::FORWARD: return "Forward";
    case Key::CLOSECD: return "Closed";
    case Key::EJECTCD: return "Ejected";
    case Key::EJECTCLOSECD: return "EjectCloseCD";
    case Key::NEXTSONG: return "NextSong";
    case Key::PLAYPAUSE: return "PlayPause";
    case Key::PREVIOUSSONG: return "PreviousSong";
    case Key::STOPCD: return "StopCD";
    case Key::RECORD: return "Record";
    case Key::REWIND: return "Rewind";
    case Key::PHONE: return "Phone";
    case Key::ISO: return "ISO";
    case Key::CONFIG: return "Config";
    case Key::HOMEPAGE: return "Homepage";
    case Key::REFRESH: return "Refresh";
    case Key::EXIT: return "Exit";
    case Key::MOVE: return "Move";
    case Key::EDIT: return "Edit";
    case Key::SCROLLUP: return "ScrollUp";
    case Key::SCROLLDOWN: return "ScrollDown";
    case Key::KPLEFTPAREN: return "KPLeftParen";
    case Key::KPRIGHTPAREN: return "KPRightParen";
    case Key::NEW: return "New";
    case Key::REDO: return "Redo";
    case Key::F13: return "F13";
    case Key::F14: return "F14";
    case Key::F15: return "F15";
    case Key::F16: return "F16";
    case Key::F17: return "F17";
    case Key::F18: return "F18";
    case Key::F19: return "F19";
    case Key::F20: return "F20";
    case Key::F21: return "F21";
    case Key::F22: return "F22";
    case Key::F23: return "F23";
    case Key::F24: return "F24";
    case Key::PLAYCD: return "PlayCD";
    case Key::PAUSECD: return "PauseCD";
    case Key::PROG3: return "Prog3";
    case Key::PROG4: return "Prog4";
    case Key::DASHBOARD: return "Dashboard";
    case Key::SUSPEND: return "Suspend";
    case Key::CLOSE: return "Close";
    case Key::PLAY: return "Play";
    case Key::FASTFORWARD: return "FastForward";
    case Key::BASSBOOST: return "BassBoost";
    case Key::PRINT: return "Print";
    case Key::HP: return "HP";
    case Key::CAMERA: return "Camera";
    case Key::SOUND: return "Sound";
    case Key::QUESTION: return "Question";
    case Key::EMAIL: return "Email";
    case Key::CHAT: return "Chat";
    case Key::SEARCH: return "Search";
    case Key::CONNECT: return "Connect";
    case Key::FINANCE: return "Finance";
    case Key::SPORT: return "Sport";
    case Key::SHOP: return "Shop";
    case Key::ALTERASE: return "AltErase";
    case Key::CANCEL: return "Cancel";
    case Key::BRIGHTNESSDOWN: return "BrightnessDown";
    case Key::BRIGHTNESSUP: return "BrightnessUp";
    case Key::MEDIA: return "Media";
    case Key::SWITCHVIDEOMODE: return "SwitchVideoMode";
    case Key::KBDILLUMTOGGLE: return "KPDillumToggle";
    case Key::KBDILLUMDOWN: return "KPDillumDown";
    case Key::KBDILLUMUP: return "KPDillumUp";
    case Key::SEND: return "Send";
    case Key::REPLY: return "Reply";
    case Key::FORWARDMAIL: return "ForwardMail";
    case Key::SAVE: return "Save";
    case Key::DOCUMENTS: return "Documents";
    case Key::BATTERY: return "Battery";
    case Key::BLUETOOTH: return "Bluetooth";
    case Key::WLAN: return "WLAN";
    case Key::UWB: return "UWB";
    case Key::UNKNOWN: return "Unknown";
    case Key::VIDEO_NEXT: return "VideoNext";
    case Key::VIDEO_PREV: return "VideoPrev";
    case Key::BRIGHTNESS_CYCLE: return "BrightnessCycle";
    case Key::BRIGHTNESS_ZERO: return "BrightnessZero";
    case Key::DISPLAY_OFF: return "DisplayOff";
    case Key::WIMAX: return "WIMAX";
    case Key::RFKILL: return "RFKill";
    case Key::MICMUTE: return "MicMute";

    case Key::ANY: return "Any";
    case Key::SHIFT: return "Shift";
    case Key::CTRL: return "Ctrl";
    case Key::META: return "Meta";

    case Key::VIRTUAL1: return "Virtual1";
    case Key::VIRTUAL2: return "Virtual2";
    case Key::VIRTUAL3: return "Virtual3";
    case Key::VIRTUAL4: return "Virtual4";
    case Key::VIRTUAL5: return "Virtual5";
    case Key::VIRTUAL6: return "Virtual6";
    case Key::VIRTUAL7: return "Virtual7";
    case Key::VIRTUAL8: return "Virtual8";

    case Key::NONE:
    case Key::COUNT:
      break;
  }
  return "???";
}

Key get_key_by_name(std::string name) {
  // generate vector of all key name/key pairs, sorted by name
  static const auto s_key_map =
    []() {
      const auto count = static_cast<int>(Key::COUNT);
      auto map = std::vector<std::pair<std::string, Key>>();
      map.reserve(count);

      for (auto k = 1; k < count; k++) {
        const auto key = static_cast<Key>(k);
        map.emplace_back(to_upper(get_key_name(key)), key);
      }

      std::sort(begin(map), end(map),
        [](const auto& a, const auto& b) { return a.first < b.first; });

      return map;
    }();

  name = to_upper(std::move(name));

  // binary search for key name
  auto it = std::lower_bound(cbegin(s_key_map), cend(s_key_map), name,
    [](const auto& kv, const auto& name) { return kv.first < name; });
  if (it != cend(s_key_map) && it->first == name)
    return it->second;

  return Key::NONE;
}
