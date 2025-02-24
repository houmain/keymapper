#pragma once

#include "StringTyperImpl.h"

struct xkb_context;
struct xkb_keymap;

class StringTyperXKB : public StringTyperImpl {
protected:
  bool update_layout_xkbcommon(xkb_context* context, xkb_keymap* keymap);
};

Key xkb_keyname_to_key(const char* name);
StringTyper::Modifiers get_xkb_modifiers(uint32_t mask);
