#pragma once

#if !defined(NDEBUG)
#include "common/output.h"
# include "runtime/KeyEvent.h"
# include "config/get_key_name.cpp"

void verbose_debug_io(const KeyEvent& input,
    const KeySequence& output, bool translated) {

  const auto format = [](const KeyEvent& e) {
    const auto key_name = [](Key key) {
      const auto name = get_key_name(key);
      return std::string(name ? name : "???");
    };
    return (e.state == KeyState::Down ? "+" :
            e.state == KeyState::Up ? "-" : "*") + key_name(e.key);
  };

  const auto input_string = format(input);
  auto output_string = std::string();
  for (const auto& event : output)
    output_string += format(event) + " ";

  verbose(translated ? "%s --> %s" : "%s",
    input_string.c_str(), output_string.c_str());
}

#else
# define verbose_debug_io(input, output, translated)
#endif // !defined(NDEBUG)
