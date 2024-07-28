#pragma once

#include <string>

#if !defined(_WIN32)

#include <wordexp.h>

inline std::string expand_path(std::string string) {
  auto exp_result = wordexp_t{ };
  wordexp(string.c_str(), &exp_result, 0);
  string = exp_result.we_wordv[0];
  wordfree(&exp_result);
  return string;
}

#else // !_WIN32

inline std::string expand_path(std::string string) {
  return string;
}

#endif // !_WIN32
