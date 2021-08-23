
#include "ServerPort.h"
#include "config/Config.h"
#include "../common.h"
#include <csignal>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

namespace {
  bool send_key_sequence(int fd, const KeySequence& sequence) {
    auto succeeded = send(fd, static_cast<uint8_t>(sequence.size()));
    for (const auto& event : sequence) {
      succeeded &= send(fd, event.key);
      succeeded &= send(fd, event.state);
    }
    return succeeded;
  }

  bool send_config(int fd, const Config& config) {
    // send mappings
    auto succeeded = send(fd, static_cast<uint16_t>(config.commands.size()));
    for (const auto& command : config.commands) {
      succeeded &= send_key_sequence(fd, command.input);
      succeeded &= send_key_sequence(fd, command.default_mapping);
    }

    // send mapping overrides
    // for each context find the mappings belonging to it
    succeeded &= send(fd, static_cast<uint16_t>(config.contexts.size()));
    auto context_mappings = std::vector<std::pair<int, const ContextMapping*>>();
    for (auto i = 0u; i < config.contexts.size(); ++i) {
      context_mappings.clear();
      for (auto j = 0u; j < config.commands.size(); ++j) {
        const auto& command = config.commands[j];
        for (const auto& context_mapping : command.context_mappings)
          if (context_mapping.context_index == static_cast<int>(i))
            context_mappings.emplace_back(j, &context_mapping);
      }
      succeeded &= send(fd, static_cast<uint16_t>(context_mappings.size()));
      for (const auto& mapping : context_mappings) {
        succeeded &= send(fd, static_cast<uint16_t>(mapping.first));
        succeeded &= send_key_sequence(fd, mapping.second->output);
      }
    }
    return succeeded;
  }
} // namespace

ServerPort::~ServerPort() {
  if (m_socket_fd >= 0)
    ::close(m_socket_fd);
}

bool ServerPort::initialize(const char* ipc_id) {
  ::signal(SIGPIPE, [](int) { });

  m_socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (m_socket_fd < 0)
    return false;

  auto addr = sockaddr_un{ };
  addr.sun_family = AF_UNIX;
  ::strncpy(&addr.sun_path[1], ipc_id, sizeof(addr.sun_path) - 2);

  for (;;) {
    if (::connect(m_socket_fd, reinterpret_cast<sockaddr*>(&addr),
        sizeof(sockaddr_un)) == 0)
      return true;

    ::usleep(50 * 1000);
  }
}

bool ServerPort::send_config(const Config& config) {
  return ::send_config(m_socket_fd, config);
}

bool ServerPort::send_active_override_set(int index) {
  return send(m_socket_fd, static_cast<uint32_t>(index));
}

bool ServerPort::receive_triggered_action(int timeout_ms, int* triggered_action) {
  if (!select(m_socket_fd, timeout_ms))
    return true;

  auto index = uint32_t{ };
  if (!read(m_socket_fd, &index))
    return false;

  *triggered_action = static_cast<int>(index);
  return true;
}
