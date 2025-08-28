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

#include <regex>

inline std::string expand_path(std::string string) {
  if (auto pos = string.find('~'); pos != std::string::npos)
    string.replace(pos, pos + 1, "${HOME}");

  static const auto s_variable_regex = std::regex("\\$(\\w+|\\{\\w+\\})");
  auto match = std::smatch();
  while (std::regex_search(string, match, s_variable_regex)) {
    auto var = match[1].str();
    if (var.front() == '{' && var.back() == '}')
      var = var.substr(1, var.size() - 2);
    const auto* value = getenv(var.c_str());
    string.replace(match[0].first, match[0].second, value ? value : "");
  }
  return string;
}

#endif // !_WIN32
