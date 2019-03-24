#pragma once

#include <ostream>
#include "catch.hpp"
#include "config/key_names.h"

KeySequence parse_input(const char* input);
KeySequence parse_output(const char* output);
KeySequence parse_sequence(const char* it, const char* const end);

template<size_t N>
KeySequence parse_sequence(const char(&input)[N]) {
  return parse_sequence(input, input + N - 1);
}
std::string format_sequence(const KeySequence& sequence);
