#pragma once

#if !defined(NDEBUG)
# include "common/output.h"
# include "runtime/KeyEvent.h"
# include "runtime/Timeout.h"
# include "config/get_key_name.h"
# include <sstream>

void verbose_debug_io(const KeyEvent& input,
    const KeySequence& output, bool translated) {

  const auto format = [](const KeyEvent& e) {
    if (e.key == Key::timeout)
      return (e.state == KeyState::Not ? "!" : "") +
        std::to_string(timeout_to_milliseconds(e.timeout).count()) + "ms";

    const auto key_name = [](Key key) {
      if (const auto name = get_key_name(key))
        return std::string(name);

      auto ss = std::stringstream();
      ss << std::hex << std::uppercase << *key;
      return ss.str();
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
