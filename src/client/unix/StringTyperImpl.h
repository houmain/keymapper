#pragma once

#include "config/StringTyper.h"
#include "runtime/Key.h"
#include <array>
#include <map>

class StringTyperImpl {
public:
  using AddKey = StringTyper::AddKey;

  virtual ~StringTyperImpl() = default;

  void type(std::string_view string, const AddKey& add_key) const;

protected:
  struct KeyModifier {
    Key key;
    StringTyper::Modifiers modifiers;
  };

  using Entry = std::vector<KeyModifier>;
  
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
