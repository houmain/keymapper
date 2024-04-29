
#include "Settings.h"
#include "common/output.h"
#include <optional>

#if defined(_WIN32)

#include "common/windows/win.h"

bool interpret_commandline(Settings& settings, int argc, wchar_t* argv[]) {
# define T(text) L##text

  auto timeout = std::optional<Duration>();
  for (auto i = 1; i < argc; i++) {
    const auto argument = std::wstring_view(argv[i]);
    const auto to_utf8 = wide_to_utf8;
#else // !defined(_WIN32)

bool interpret_commandline(Settings& settings, int argc, char* argv[]) {
# define T(text) text

  auto timeout = std::optional<Duration>();
  for (auto i = 1; i < argc; i++) {
    const auto argument = std::string_view(argv[i]);
    using to_utf8 = std::string;
#endif // !defined(_WIN32)

    if (argument == T("--timeout")) {
      if (++i >= argc)
        return false;

      timeout = std::chrono::milliseconds(std::stoi(to_utf8(argv[i])));
      if (timeout < std::chrono::seconds::zero() || timeout > std::chrono::hours(24))
        timeout.reset();
    }
    else if (argument == T("--instance")) {
      if (++i >= argc)
        return false;

      const auto id = to_utf8(argv[i]);
      settings.requests.push_back({ RequestType::set_instance_id, id });
    }
    else if (argument == T("--wait")) {
      if (++i >= argc)
        return false;

      timeout = std::chrono::milliseconds(std::stoi(to_utf8(argv[i])));
      settings.requests.push_back({ RequestType::wait, "", timeout });
    }
    else if (argument == T("--stdout")) {
      settings.requests.push_back({ RequestType::stdout_result });
    }
    else if (argument == T("--restart")) {
      settings.requests.push_back({ RequestType::restart, "", timeout });
    }
    else if (argument == T("--set-config")) {
      if (++i >= argc)
        return false;

      auto filename = to_utf8(argv[i]);
      settings.requests.push_back({ RequestType::set_config_file, 
        std::move(filename), timeout });
    }
    else {
      const auto request_type = [&]() -> std::optional<RequestType> {
        if (argument == T("--press")) return RequestType::press;
        if (argument == T("--release")) return RequestType::release;
        if (argument == T("--toggle")) return RequestType::toggle;
        if (argument == T("--is-pressed")) return RequestType::is_pressed;
        if (argument == T("--is-released")) return RequestType::is_released;
        if (argument == T("--wait-pressed")) return RequestType::wait_pressed;
        if (argument == T("--wait-released")) return RequestType::wait_released;
        if (argument == T("--wait-toggled")) return RequestType::wait_toggled;
        return std::nullopt;
      }();
      if (!request_type)
        return false;
      if (++i >= argc)
        return false;

      const auto key = to_utf8(argv[i]);
      settings.requests.push_back({ *request_type, key, timeout });
    }
  }
  if (settings.requests.empty())
    return false;

  return true;
}

void print_help_message() {
  message(R"(
keymapperctl %s

Usage: keymapperctl [--operation]
  --is-pressed <key>    sets the result code 0 when a virtual key is down.
  --is-released <key>   sets the result code 0 when a virtual key is up.
  --press <key>         presses a virtual key.
  --release <key>       releases a virtual key.
  --toggle <key>        toggles a virtual key.
  --wait-pressed <key>  waits until a virtual key is pressed.
  --wait-released <key> waits until a virtual key is released.
  --wait-toggled <key>  waits until a virtual key is toggled.
  --timeout <millisecs> sets a timeout for the following operation.
  --wait <millisecs>    unconditionally waits a given amount of time.
  --instance <id>       replaces another keymapperctl process with the same id.
  --restart             starts processing the first operation again.
  --stdout              writes the result code to stdout.
  --set-config <file>   sets a new configuration.
  -h, --help            print this help.

%s
)", about_header, about_footer);
}
