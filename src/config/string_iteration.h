#pragma once

#include <string>

template<typename ForwardIt>
bool skip(ForwardIt* it, ForwardIt end, const char* str) {
  auto it2 = *it;
  while (*str) {
    if (it2 == end || *str != *it2)
      return false;
    ++str;
    ++it2;
  }
  *it = it2;
  return true;
}

template<typename ForwardIt>
bool skip_until(ForwardIt* it, ForwardIt end, const char* str) {
  while (*it != end) {
    if (skip(it, end, str))
      return true;
    ++(*it);
  }
  return false;
}

template<typename ForwardIt>
bool skip_space(ForwardIt* it, ForwardIt end) {
  auto skipped = false;
  while (*it != end && isspace(**it)) {
    skipped = true;
    ++(*it);
  }
  return skipped;
}

template<typename ForwardIt>
bool skip_comments(ForwardIt* it, ForwardIt end) {
  if (skip(it, end, "#") || skip(it, end, ";")) {
    skip_until(it, end, "\n");
    return true;
  }
  return false;
}

template<typename ForwardIt>
bool skip_space_and_comments(ForwardIt* it, ForwardIt end) {
  auto skipped = false;
  while (*it != end && isspace(**it)) {
    skipped = true;
    ++(*it);
  }
  if (skip_comments(it, end))
    skipped = true;

  return skipped;
}

template<typename ForwardIt>
bool trim_comment(ForwardIt it, ForwardIt* end) {
  for (; it != *end; ++it)
    if (*it == '#' || *it == ';') {
      *end = it;
      return true;
    }
  return false;
}

template<typename ForwardIt>
void skip_value(ForwardIt* it, ForwardIt end) {
  while (*it != end && (isalnum(**it) || **it == '_' || **it == '.'))
    ++(*it);
}

template<typename ForwardIt>
void skip_ident(ForwardIt* it, ForwardIt end) {
  while (*it != end && (isalnum(**it) || **it == '_'))
    ++(*it);
}

template<typename ForwardIt>
std::string read_value(ForwardIt* it, ForwardIt end) {
  const auto begin = *it;
  if (skip(it, end, "'") || skip(it, end, "\"")) {
    const char mark[2] = { *(*it - 1), '\0' };
    if (skip_until(it, end, mark))
      return std::string(begin + 1, *it - 1);
    return std::string();
  }
  skip_value(it, end);
  return std::string(begin, *it);
}

template<typename ForwardIt>
std::string read_ident(ForwardIt* it, ForwardIt end) {
  const auto begin = *it;
  skip_ident(it, end);
  return std::string(begin, *it);
}
