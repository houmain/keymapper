
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
  
  struct SendResult {
    Result result;
    std::optional<KeyState> state;
  };
  
  Settings g_settings;
  ClientPort g_client;

  SendResult read_virtual_key_state(std::optional<Duration> timeout) {
    auto state = std::optional<KeyState>();
    if (!g_client.read_virtual_key_state(timeout, &state))
      return { Result::connection_failed };
    if (!state.has_value())
      return { Result::timeout };
    return { Result::yes, state };
  }

  SendResult set_key_state(std::string_view key, KeyState state, 
      std::optional<Duration> timeout) {
    if (!g_client.send_set_virtual_key_state(key, state))
      return { Result::connection_failed };
    return read_virtual_key_state(timeout);
  }

  SendResult get_key_state(std::string_view key, 
      std::optional<Duration>timeout) {
    if (!g_client.send_get_virtual_key_state(key))
      return { Result::connection_failed };
    return read_virtual_key_state(timeout);
  }

  SendResult wait_until_key_state_changed(std::string_view key, 
      std::optional<Duration>timeout) {
    if (!g_client.send_request_virtual_key_toggle_notification(key))
      return { Result::connection_failed };
    return read_virtual_key_state(timeout);
  }
  
  Result set_instance_id(const std::string& id, 
      std::optional<Duration>timeout) {
    if (!g_client.send_set_instance_id(id))
      return Result::connection_failed;
    return read_virtual_key_state(timeout).result;
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
      case RequestType::toggle: {
        const auto [res, state] = set_key_state(request.key,
            (request.type == RequestType::press ? KeyState::Down :
             request.type == RequestType::release ? KeyState::Up :
             KeyState::Not), request.timeout);
        if (result == Result::yes) {
          if (state == KeyState::Up || state == KeyState::Down) {
            result = Result::yes;
          }
          else {
            result = Result::key_not_found;
          }
        }
        else {
          result = res;
        }
        break;
      }

      case RequestType::is_pressed:
      case RequestType::is_released: {
        const auto [res, state] = get_key_state(request.key, request.timeout);
        if (res == Result::yes) {
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
          result = res;
        }
        break;
      }

      case RequestType::wait_pressed:
      case RequestType::wait_released:
      case RequestType::wait_toggled: {
        const auto [res, state] = get_key_state(request.key, request.timeout);
        if (res == Result::yes) {
          if ((request.type == RequestType::wait_pressed && state == KeyState::Down) || 
              (request.type == RequestType::wait_released && state == KeyState::Up)) {
            result = Result::yes;
          }
          else {
            result = wait_until_key_state_changed(request.key, request.timeout).result;
          }
        }
        else {
          result = res;
        }
        break;
      }

      case RequestType::wait:
        std::this_thread::sleep_for(*request.timeout);
        break;

      case RequestType::set_instance_id:
        result = set_instance_id(request.key, request.timeout);
        break;

      case RequestType::stdout_result:
        std::fputc('0' + static_cast<int>(result), stdout);
        std::fflush(stdout);
        break;

      case RequestType::restart:
        if (result != Result::connection_failed) {
          request_index = 0;
          continue;
        }
        break;
    }
    ++request_index;
  }
  return result;
}
