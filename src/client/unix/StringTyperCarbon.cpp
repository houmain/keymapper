
#if defined(ENABLE_CARBON)

#include "StringTyperImpl.h"
#include <Carbon/Carbon.h>

namespace {
  Key vk_to_key(UInt16 vk) {
    switch (vk) {
      case kVK_ANSI_A: return Key::A;
      case kVK_ANSI_S: return Key::S;
      case kVK_ANSI_D: return Key::D;
      case kVK_ANSI_F: return Key::F;
      case kVK_ANSI_H: return Key::H;
      case kVK_ANSI_G: return Key::G;
      case kVK_ANSI_Z: return Key::Z;
      case kVK_ANSI_X: return Key::X;
      case kVK_ANSI_C: return Key::C;
      case kVK_ANSI_V: return Key::V;
      case kVK_ANSI_B: return Key::B;
      case kVK_ANSI_Q: return Key::Q;
      case kVK_ANSI_W: return Key::W;
      case kVK_ANSI_E: return Key::E;
      case kVK_ANSI_R: return Key::R;
      case kVK_ANSI_Y: return Key::Y;
      case kVK_ANSI_T: return Key::T;
      case kVK_ANSI_1: return Key::Digit1;
      case kVK_ANSI_2: return Key::Digit2;
      case kVK_ANSI_3: return Key::Digit3;
      case kVK_ANSI_4: return Key::Digit4;
      case kVK_ANSI_6: return Key::Digit6;
      case kVK_ANSI_5: return Key::Digit5;
      case kVK_ANSI_Equal: return Key::Equal;
      case kVK_ANSI_9: return Key::Digit9;
      case kVK_ANSI_7: return Key::Digit7;
      case kVK_ANSI_Minus: return Key::Minus;
      case kVK_ANSI_8: return Key::Digit8;
      case kVK_ANSI_0: return Key::Digit0;
      case kVK_ANSI_RightBracket: return Key::BracketRight;
      case kVK_ANSI_O: return Key::O;
      case kVK_ANSI_U: return Key::U;
      case kVK_ANSI_LeftBracket: return Key::BracketLeft;
      case kVK_ANSI_I: return Key::I;
      case kVK_ANSI_P: return Key::P;
      case kVK_ANSI_L: return Key::L;
      case kVK_ANSI_J: return Key::J;
      case kVK_ANSI_Quote: return Key::Quote;
      case kVK_ANSI_K: return Key::K;
      case kVK_ANSI_Semicolon: return Key::Semicolon;
      case kVK_ANSI_Backslash: return Key::Backslash;
      case kVK_ANSI_Comma: return Key::Comma;
      case kVK_ANSI_Slash: return Key::Slash;
      case kVK_ANSI_N: return Key::N;
      case kVK_ANSI_M: return Key::M;
      case kVK_ANSI_Period: return Key::Period;
      case kVK_ANSI_Grave: return Key::IntlBackslash;
      case kVK_Return: return Key::Enter;
      case kVK_Tab: return Key::Tab;
      case kVK_Space: return Key::Space;
    }
    return Key::none;
  }

  StringTyper::Modifiers get_modifiers(UInt32 modifier) {
    auto modifiers = StringTyper::Modifiers{ };
    if (modifier & shiftKey) modifiers |= StringTyper::Shift;
    if (modifier & optionKey) modifiers |= StringTyper::Alt;
    if (modifier & controlKey) modifiers |= StringTyper::Control;
    return modifiers;
  }
} // namespace

class StringTyperCarbon : public StringTyperImpl {
public:
  bool update_layout() {
    const auto source = TISCopyCurrentKeyboardLayoutInputSource();
    const auto layout_ref = static_cast<CFDataRef>(
      TISGetInputSourceProperty(source, kTISPropertyUnicodeKeyLayoutData));
    const auto layout = reinterpret_cast<const UCKeyboardLayout*>(CFDataGetBytePtr(layout_ref));
    const auto keyboard_type = LMGetKbdType();

    m_dictionary.clear();
    for_each_modifier_combination(std::array{ shiftKey, optionKey, controlKey }, [&](auto modifier) {
      for (auto vk = UInt16{ }; vk < 0xFF; ++vk)
        if (const auto key = vk_to_key(vk); key != Key::none) {
          auto modifiers = (static_cast<UInt32>(modifier) >> 8);
          auto dead_key_state = UInt32{ };
          auto buffer = std::array<UniChar, 8>{};
          auto length = UniCharCount{ };
          if (UCKeyTranslate(layout, vk, kUCKeyActionDown, modifiers, keyboard_type, 
                kUCKeyTranslateNoDeadKeysBit, &dead_key_state, buffer.size(), &length, buffer.data()) == noErr &&
              length > 0) {
            const auto utf32 = utf16_to_utf32(std::u16string_view(
              reinterpret_cast<const char16_t*>(buffer.data()), length));  
            if (auto it = m_dictionary.find(utf32[0]); it == m_dictionary.end())
              m_dictionary[utf32[0]] = { key, get_modifiers(modifier) };
          }
        }
    });

    CFRelease(source);
    return true;
  }
};

std::unique_ptr<StringTyperImpl> make_string_typer_carbon() {
  auto impl = std::make_unique<StringTyperCarbon>();
  if (!impl->update_layout())
    return { };
  return impl;
}

#endif // ENABLE_CARBON
