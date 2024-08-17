#pragma once

#include <string>
#include <cctype>
#include <optional>
#include <stdexcept>

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
bool skip(ForwardIt* it, ForwardIt end, char c) {
  const char mark[2] = { c, '\0' };
  return skip(it, end, mark);
}

template<typename ForwardIt>
bool skip_until(ForwardIt* it, ForwardIt end, const char* str) {
  const auto begin = *it;
  while (*it != end) {
    if (skip(it, end, str))
      return true;
    ++(*it);
  }
  *it = begin;
  return false;
}

template<typename ForwardIt>
bool skip_until(ForwardIt* it, ForwardIt end, char c) {
  const char mark[2] = { c, '\0' };
  return skip_until(it, end, mark);
}

template<typename ForwardIt>
bool skip_until_not_in_string(ForwardIt* it, ForwardIt end, const char* str) {
  for (;;) {
    if (skip(it, end, '"') || skip(it, end, '\'')) {
      if (!skip_until(it, end, *std::prev(*it)))
        throw std::runtime_error("Unterminated string");
    }
    if (skip(it, end, str))
      return true;
    if (*it == end)
      return false;
    ++(*it);
  }
}

template<typename ForwardIt>
bool skip_until_not_in_string(ForwardIt* it, ForwardIt end, char c) {
  const char mark[2] = { c, '\0' };
  return skip_until_not_in_string(it, end, mark);
}

template<typename ForwardIt>
bool skip_comments(ForwardIt* it, ForwardIt end) {
  if (*it == end)
    return false;

  const auto firstchar = static_cast<unsigned char>(**it);
  if (firstchar == '#' || firstchar == ';') {
    *it = end;
    return true;
  }
  return false;
}

template<typename ForwardIt>
bool skip_space(ForwardIt* it, ForwardIt end) {
  auto skipped = false;
  while (*it != end && std::isspace(static_cast<unsigned char>(**it))) {
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
void trim_space(ForwardIt begin, ForwardIt* end) {
  while (*end != begin &&
         std::isspace(static_cast<unsigned char>(*std::prev(*end))))
    --(*end);
}

template<typename ForwardIt>
bool skip_ident(ForwardIt* it, ForwardIt end) {
  const auto begin = *it;
  while (*it != end && 
     (std::isalnum(static_cast<unsigned char>(**it)) || 
      **it == '_' ||
      **it == '-'))
    ++(*it);
  return (*it != begin);
}

template<typename ForwardIt>
std::string read_value(ForwardIt* it, ForwardIt end) {
  const auto begin = *it;
  if (skip(it, end, "'") || skip(it, end, "\"")) {
    if (!skip_until(it, end, *std::prev(*it)))
      throw std::runtime_error("Unterminated string");
    return std::string(std::next(begin), std::prev(*it));
  }
  skip_ident(it, end);
  return std::string(begin, *it);
}

template<typename ForwardIt>
std::string read_ident(ForwardIt* it, ForwardIt end) {
  const auto begin = *it;
  skip_ident(it, end);
  return std::string(begin, *it);
}

template<typename ForwardIt>
std::optional<int> try_read_number(ForwardIt* it, ForwardIt end) {
  const auto begin = *it;
  auto number = 0;
  for (; *it != end && **it >= '0' && **it <= '9'; ++*it)
    number = number * 10 + (**it - '0');
  return (*it != begin ? std::make_optional(number) : std::nullopt);
}

template<typename ForwardIt>
bool skip_arglist(ForwardIt* it, ForwardIt end) {
  const auto begin = *it;
  if (!skip(it, end, "["))
    return false;

  auto level = 1;
  for (;;) {
    auto next_close = *it;
    if (!skip_until_not_in_string(&next_close, end, "]"))
      break;

    auto next_open = *it; 
    if (skip_until_not_in_string(&next_open, end, "["))
      if (next_open < next_close) {
        *it = next_open;
        ++level;
        continue;
      }

    *it = next_close;
    if (--level == 0)
      return true;
  }
  *it = begin;
  return false;
}
