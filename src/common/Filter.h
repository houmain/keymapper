#pragma once

#include <string>
#include <optional>
#include <regex>

struct Filter {
  std::string string;
  std::optional<std::regex> regex;
  bool invert{ };

  bool matches(const std::string& text, bool substring) const {
    return matches_uninverted(text, substring) ^ invert;
  }

  bool matches_uninverted(const std::string& text, bool substring) const {
    if (string.empty())
      return true;
    if (regex.has_value())
      return std::regex_search(text, *regex);
    return (substring ?
      text.find(string) != std::string::npos :
      text == string);
  }

  explicit operator bool() const {
    return !string.empty();
  }
};

struct GrabDeviceFilter : Filter {
  bool by_id{ };
};

inline bool evaluate_grab_filters(const std::vector<GrabDeviceFilter>& filters,
    const std::string& device_name, const std::string& device_id,
    bool grab) {

  for (const auto& filter : filters)
    if (filter.matches_uninverted(filter.by_id ? device_id : device_name, false))
      grab = !filter.invert;

  return grab;
}
