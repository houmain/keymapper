#pragma once

#include "runtime/Key.h"
#include <memory>
#include <string>
#include <functional>

class StringTyper {
public:
  enum Modifier {
    Shift   = (1 << 0),
    Alt     = (1 << 1),
    AltGr   = (1 << 2),
    Control = (1 << 3),
  };
  using Modifiers = int;
  using AddKey = std::function<void(Key, Modifiers)>;

  StringTyper();
  StringTyper(StringTyper&& rhs) noexcept;
  StringTyper& operator=(StringTyper&& rhs) noexcept;
  ~StringTyper();

  void type(std::string_view string, const AddKey& add_key) const;

private:
  std::unique_ptr<class StringTyperImpl> m_impl;
};
