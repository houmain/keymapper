
#include "config/StringTyper.h"
#include "common/output.h"
#include "common/windows/win.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <map>

namespace {
  std::vector<std::pair<UINT, UINT>> get_vk_scan_codes() {
    auto keys = std::vector<std::pair<UINT, UINT>>{ };
    for (auto vk : std::initializer_list<UINT>{
        '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', VK_OEM_4, VK_OEM_6, VK_TAB, 
        'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', VK_OEM_1, VK_OEM_PLUS, VK_RETURN, 
        'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', VK_OEM_3, VK_OEM_7, VK_OEM_5, VK_OEM_2, 
        'Z', 'X', 'C', 'V', 'B', 'N', 'M', VK_OEM_COMMA, VK_OEM_PERIOD, VK_OEM_MINUS, VK_SPACE, VK_OEM_102, 
      })
      keys.emplace_back(vk, MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
    return keys;
  }
} // namespace

class StringTyperImpl {
private:
  struct KeyModifier {
    Key key;
    StringTyper::Modifiers modifiers;
  };

  struct Entry {
    KeyModifier dead_key;
    KeyModifier key;
  };

  using AddKey = StringTyper::AddKey;
  const HKL m_layout = GetKeyboardLayout(0);
  std::map<WCHAR, Entry> m_dictionary;

public:
  StringTyperImpl() {
    const auto vk_scan_codes_list = get_vk_scan_codes();
    const auto modifier_list = std::initializer_list<StringTyper::Modifiers>{ { }, 
      StringTyper::Shift, StringTyper::AltGr };

    auto buffer = std::array<WCHAR, 8>{ };
    auto key_state = std::array<BYTE, 256>{ };
    const auto flags = 0x00;

    const auto toggle_modifier = [&](StringTyper::Modifiers modifier, bool set) {
      const auto value = (set ? BYTE{ 0x80 } : BYTE{ 0x00 });
      if (modifier == StringTyper::Shift) {
        key_state[VK_SHIFT] = value;
      } 
      else if (modifier == StringTyper::AltGr) {
        key_state[VK_MENU] = value;
        key_state[VK_CONTROL] = value;
      }
    };

    auto dead_keys = std::vector<std::tuple<UINT, UINT, StringTyper::Modifiers>>();
    dead_keys.push_back({ });
    for (auto modifier : modifier_list)
      for (const auto [vk_code, scan_code] : vk_scan_codes_list) {
        toggle_modifier(modifier, true);
        auto result = ToUnicodeEx(vk_code, scan_code, key_state.data(), 
            buffer.data(), static_cast<int>(buffer.size()), flags, m_layout);
        toggle_modifier(modifier, false);

        if (result < 0) {
          dead_keys.push_back({ vk_code, scan_code, modifier });

          // clear dead key state
          result = ToUnicodeEx(vk_code, scan_code, key_state.data(), 
            buffer.data(), static_cast<int>(buffer.size()), flags, m_layout);
          assert(result >= 0);
        }
      }

    for (auto [dead_key_code, dead_key_scan_code, dead_key_modifier] : dead_keys)
      for (auto modifier : modifier_list)
        for (const auto [vk_code, scan_code] : vk_scan_codes_list) {

          if (dead_key_code) {
            toggle_modifier(dead_key_modifier, true);
            const auto result = ToUnicodeEx(dead_key_code, dead_key_scan_code, key_state.data(), 
              buffer.data(), static_cast<int>(buffer.size()), flags, m_layout);
            toggle_modifier(dead_key_modifier, false);
          }
          else {
            // do not start with dead key
            if (std::count_if(std::begin(dead_keys), std::end(dead_keys), 
                [&](const auto& tuple) { 
                  const auto [dead_key_code, dead_key_scan_code, dead_key_modifier] = tuple;
                  return (dead_key_code == vk_code && dead_key_modifier == modifier); 
                }))
              continue;
          }

          toggle_modifier(modifier, true);
          auto result = ToUnicodeEx(vk_code, scan_code, key_state.data(), 
              buffer.data(), static_cast<int>(buffer.size()), flags, m_layout);
          toggle_modifier(modifier, false);

          assert(result >= 0);
          if (result == 1) {
            m_dictionary.emplace(buffer[0], Entry{ 
              { static_cast<Key>(dead_key_scan_code), dead_key_modifier }, 
              { static_cast<Key>(scan_code), modifier } 
            });
          }
          else if (result == 0 && dead_key_code) {
            // clear dead key state
            result = ToUnicodeEx(vk_code, scan_code, key_state.data(), 
              buffer.data(), static_cast<int>(buffer.size()), flags, m_layout);
            assert(result > 0);
          }
        }
  }

  void type(std::string_view string, const AddKey& add_key) const {
    auto characters = utf8_to_wide(string);
    replace_all<wchar_t>(characters, L"\\n", L"\r");
    replace_all<wchar_t>(characters, L"\\r", L"\r");
    replace_all<wchar_t>(characters, L"\\t", L"\t");

    for (auto character : characters) {
      if (auto it = m_dictionary.find(character); it != m_dictionary.end()) {
        const auto& [dead_key, key] = it->second;
        if (dead_key.key != Key::none)
          add_key(dead_key.key, dead_key.modifiers);
        add_key(key.key, key.modifiers);
      }
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
