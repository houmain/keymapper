#pragma once

#include "config/StringTyper.h"
#include "runtime/Key.h"
#include <array>
#include <map>

class StringTyperImpl {
public:
  struct KeyModifier {
    Key key;
    StringTyper::Modifiers modifiers;
  };
  using Modifier = StringTyper::Modifier;
  using Modifiers = StringTyper::Modifiers;
  using Entry = std::vector<KeyModifier>;
  using AddKey = StringTyper::AddKey;
  
  static KeyModifier s_compose_key;

  virtual ~StringTyperImpl() = default;

  void type(std::string_view string, const AddKey& add_key) const;

protected:
  std::map<char32_t, Entry> m_dictionary;
};

std::u32string utf8_to_utf32(std::string_view utf8_string);
std::u32string utf16_to_utf32(std::u16string_view utf16_string);

template<typename T, typename F>
void for_each_modifier_combination(const T& masks, F&& callback) {
  for (auto bit = 0x00; bit < (1 << masks.size()); ++bit) {
    auto combo = 0x00;
    for (auto i = 0u; i < masks.size(); ++i)
      if (bit & (1 << i))
        combo |= masks[i];
    callback(combo);
  }
}
