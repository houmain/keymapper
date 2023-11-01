
#include "config/StringTyper.h"
#include "common/output.h"
#include "common/windows/win.h"

namespace {
  Key vk_to_key(uint8_t vk) {
    switch (vk) {
      case '1': return Key::Digit1;
      case '2': return Key::Digit2;
      case '3': return Key::Digit3;
      case '4': return Key::Digit4;
      case '5': return Key::Digit5;
      case '6': return Key::Digit6;
      case '7': return Key::Digit7;
      case '8': return Key::Digit8;
      case '9': return Key::Digit9;
      case '0': return Key::Digit0;
      case VK_OEM_4: return Key::Minus;
      case VK_OEM_6: return Key::Equal;
      case VK_TAB: return Key::Tab;
      case 'Q': return Key::KeyQ;
      case 'W': return Key::KeyW;
      case 'E': return Key::KeyE;
      case 'R': return Key::KeyR;
      case 'T': return Key::KeyT;
      case 'Y': return Key::KeyY;
      case 'U': return Key::KeyU;
      case 'I': return Key::KeyI;
      case 'O': return Key::KeyO;
      case 'P': return Key::KeyP;
      case VK_OEM_1: return Key::BracketLeft;
      case VK_OEM_PLUS: return Key::BracketRight;
      case VK_RETURN: return Key::Enter;
      case 'A': return Key::KeyA;
      case 'S': return Key::KeyS;
      case 'D': return Key::KeyD;
      case 'F': return Key::KeyF;
      case 'G': return Key::KeyG;
      case 'H': return Key::KeyH;
      case 'J': return Key::KeyJ;
      case 'K': return Key::KeyK;
      case 'L': return Key::KeyL;
      case VK_OEM_3: return Key::Semicolon;
      case VK_OEM_7: return Key::Quote;
      case VK_OEM_5: return Key::Backquote;
      case VK_OEM_2: return Key::Backslash;
      case 'Z': return Key::KeyZ;
      case 'X': return Key::KeyX;
      case 'C': return Key::KeyC;
      case 'V': return Key::KeyV;
      case 'B': return Key::KeyB;
      case 'N': return Key::KeyN;
      case 'M': return Key::KeyM;
      case VK_OEM_COMMA: return Key::Comma;
      case VK_OEM_PERIOD: return Key::Period;
      case VK_OEM_MINUS: return Key::Slash;
      case VK_SPACE: return Key::Space;
      case VK_OEM_102: return Key::IntlBackslash;
    }
    return Key::none;
  }

  std::wstring utf8_to_wide(std::string_view str) {
    auto result = std::wstring();
    result.resize(MultiByteToWideChar(CP_UTF8, 0, 
      str.data(), static_cast<int>(str.size()), 
      NULL, 0));
    MultiByteToWideChar(CP_UTF8, 0, 
      str.data(), static_cast<int>(str.size()), 
      result.data(), static_cast<int>(result.size()));
    return result;
  }

  StringTyper::Modifiers get_modifiers(int vk_modifiers) {
    switch (vk_modifiers) {
      case 1: return StringTyper::Shift;
      case 4: return StringTyper::Alt;
      case 6: return StringTyper::AltGr;
      default: return 0;
    }
  }
} // namespace

class StringTyperImpl {
private:
  using AddKey = StringTyper::AddKey;
  const HKL m_layout = GetKeyboardLayout(0);

public:
  void type(std::string_view string, const AddKey& add_key) const {
    for (auto c : utf8_to_wide(string))
      if (const auto result = VkKeyScanExW(c, m_layout); result > 0) {
        const auto key = vk_to_key(result & 0xFF);
        const auto modifiers = static_cast<uint8_t>(result >> 8);
        if (key != Key::none)
          add_key(key, get_modifiers(modifiers));
      }
  }
};

//-------------------------------------------------------------------------

StringTyper::StringTyper() 
  : m_impl(std::make_unique<StringTyperImpl>()) {
}

StringTyper::StringTyper(StringTyper&& rhs) noexcept = default;
StringTyper& StringTyper::operator=(StringTyper&& rhs) noexcept = default;
StringTyper::~StringTyper() = default;

void StringTyper::type(std::string_view string, const AddKey& add_key) const {
  m_impl->type(string, add_key);
}
