#pragma once

#include "config/StringTyper.h"

class StringTyperImpl {
public:
  using AddKey = StringTyper::AddKey;

  virtual ~StringTyperImpl() = default;

  virtual void type(std::string_view string, const AddKey& add_key) const = 0;
};
