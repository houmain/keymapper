
#include "control/Settings.h"
#include "control/ClientPort.h"
#include <thread>
#include <filesystem>

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

  SendResult set_config_file(const std::string& filename, 
      std::optional<Duration>timeout) {
    auto error = std::error_code{ };
    const auto absolute = std::filesystem::absolute(filename, error);
    if (error)
      return { Result::key_not_found };
    if (!g_client.send_set_config_file(absolute.string()))
      return { Result::connection_failed };
    return read_virtual_key_state(timeout);
  }

  Result make_request(const Request& request, const Result& last_result) {
    switch (request.type) {
      case RequestType::press:
      case RequestType::release:
      case RequestType::toggle: {
        const auto [result, state] = set_key_state(request.key,
            (request.type == RequestType::press ? KeyState::Down :
             request.type == RequestType::release ? KeyState::Up :
             KeyState::Not), request.timeout);
        if (result == Result::yes) {
          if (state == KeyState::Up || state == KeyState::Down)
            return Result::yes;
          return Result::key_not_found;
        }
        return result;
      }

      case RequestType::is_pressed:
      case RequestType::is_released: {
        const auto [result, state] = get_key_state(request.key, request.timeout);
        if (result == Result::yes) {
          if (state == KeyState::Down)
            return (request.type == RequestType::is_pressed ?
              Result::yes : Result::no);
          
          if (state == KeyState::Up)
            return (request.type == RequestType::is_released ?
              Result::yes : Result::no);
          
          return Result::key_not_found;
        }
        return result;
      }

      case RequestType::wait_pressed:
      case RequestType::wait_released:
      case RequestType::wait_toggled: {
        const auto [result, state] = get_key_state(request.key, request.timeout);
        if (result == Result::yes) {
          if ((request.type == RequestType::wait_pressed && state == KeyState::Down) || 
              (request.type == RequestType::wait_released && state == KeyState::Up))
            return Result::yes;

          return wait_until_key_state_changed(request.key, request.timeout).result;
        }
        return result;
      }

      case RequestType::wait:
        std::this_thread::sleep_for(*request.timeout);
        break;

      case RequestType::set_instance_id:
        return set_instance_id(request.key, request.timeout);

      case RequestType::stdout_result:
        std::fputc('0' + static_cast<int>(last_result), stdout);
        std::fflush(stdout);
        break;

      case RequestType::restart:
        break;

      case RequestType::set_config_file: {
        const auto [result, state] = set_config_file(request.key, request.timeout);
        return (result != Result::yes ? result :
          (state == KeyState::Down ? Result::yes : Result::no));
      }
    }
    return last_result;
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

  auto result = Result::connection_failed;
  auto connect_timeout = g_settings.requests.front().timeout;

RESTART:
  if (result == Result::connection_failed)
    if (!g_client.connect(connect_timeout))
      return Result::connection_failed;

  for (const auto& request : g_settings.requests)
    if (request.type == RequestType::restart) {
      connect_timeout = request.timeout;
      goto RESTART;
    }
    else {
      result = make_request(request, result);
    }

  return result;
}
