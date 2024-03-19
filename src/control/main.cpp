
#include "control/Settings.h"
#include "control/ClientPort.h"
#include <thread>

namespace {
  enum Result : int {
    yes                = 0,
    no                 = 1,
    invalid_arguments  = 2,
    connection_failed  = 3,
    timeout            = 4,
    key_not_found      = 5,
  };

  Settings g_settings;
  ClientPort g_client;

  std::optional<KeyState> set_key_state(std::string_view key, KeyState state, 
      std::optional<Duration> timeout) {
    g_client.send_set_virtual_key_state(key, state);
    return g_client.read_virtual_key_state(timeout);
  }

  std::optional<KeyState> get_key_state(std::string_view key, 
      std::optional<Duration>timeout) {
    g_client.send_get_virtual_key_state(key);
    return g_client.read_virtual_key_state(timeout);
  }

  std::optional<KeyState> wait_until_key_state_changed(std::string_view key, 
      std::optional<Duration>timeout) {
    g_client.send_request_virtual_key_toggle_notification(key);
    return g_client.read_virtual_key_state(timeout);
  }
} // namespace

#if defined(_WIN32)
int wmain(int argc, wchar_t** argv) {
#else
int main(int argc, char* argv[]) {
#endif
  if (!interpret_commandline(g_settings, argc, argv)) {
    print_help_message();
    return Result::invalid_arguments;
  }

  if (!g_client.connect(g_settings.requests.front().timeout))
    return Result::connection_failed;

  auto result = Result{ };
  for (auto request_index = 0u; request_index < g_settings.requests.size(); ) {  
    const auto& request = g_settings.requests[request_index];
    switch (request.type) {
      case RequestType::press:
      case RequestType::release:
      case RequestType::toggle:
        if (auto state = set_key_state(request.key,
            (request.type == RequestType::press ? KeyState::Down :
             request.type == RequestType::release ? KeyState::Up :
             KeyState::Not), request.timeout)) {
          if (state == KeyState::Up || state == KeyState::Down) {
            result = Result::yes;
          }
          else {
            result = Result::key_not_found;
          }
        }
        else {
          result = Result::timeout;
        }
        break;

      case RequestType::is_pressed:
      case RequestType::is_released:
        if (auto state = get_key_state(request.key, request.timeout)) {
          if (state == KeyState::Down) {
            result = (request.type == RequestType::is_pressed ?
              Result::yes : Result::no);
          }
          else if (state == KeyState::Up) {
            result = (request.type == RequestType::is_released ?
              Result::yes : Result::no);
          }
          else {
            result = Result::key_not_found;
          }
        }
        else {
          result = Result::timeout;
        }
        break;

      case RequestType::wait_pressed:
      case RequestType::wait_released:
      case RequestType::wait_toggled:
        if (const auto state = get_key_state(request.key, request.timeout)) {
          if (state == KeyState::Down || state == KeyState::Up) {
            if ((request.type == RequestType::wait_pressed && state == KeyState::Down) || 
                (request.type == RequestType::wait_released && state == KeyState::Up) ||
                (wait_until_key_state_changed(request.key, request.timeout))) {
              result = Result::yes;
            }
            else {
              result = Result::timeout;
            }
          }
          else {
            result = Result::key_not_found;
          }
        }
        else {
          result = Result::timeout;
        }
        break;

      case RequestType::wait:
        std::this_thread::sleep_for(*request.timeout);
        break;

      case RequestType::restart:
        request_index = 0;
        continue;
    }
    ++request_index;
  }
  return result;
}
