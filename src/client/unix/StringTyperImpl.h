#pragma once

#include "config/StringTyper.h"

class StringTyperImpl {
public:
  using AddKey = StringTyper::AddKey;

  virtual ~StringTyperImpl() = default;

  virtual void type(std::string_view string, const AddKey& add_key) const = 0;
};

std::u32string utf8_to_utf32(std::string_view utf8_string);
Key xkb_keyname_to_key(const char* name);
StringTyper::Modifiers get_xkb_modifiers(uint32_t mask);
