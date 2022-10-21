#pragma once

#include <regex>
#include <string>
#include <cassert>

inline bool is_regex(std::string_view string) {
  return (string.size() >= 2 &&
          string.front() == '/' &&
          string[string.size() - (string.back() == 'i' ? 2 : 1)] == '/');
}

inline std::regex parse_regex(std::string_view string) {
  assert(is_regex(string));
  string.remove_prefix(1);
  auto type = std::regex::ECMAScript;
  if (string.back() == 'i') {
    string.remove_suffix(1);
    type |= std::regex::icase;
  }
  string.remove_suffix(1);
  return std::regex(string.data(), string.size(), type);
}
