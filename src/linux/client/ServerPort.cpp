
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
    auto succeeded = send(fd, static_cast<uint32_t>(config.contexts.size()));
    for (const auto& context : config.contexts) {
      // inputs
      succeeded &= send(fd, static_cast<uint32_t>(context.inputs.size()));
      for (const auto& input : context.inputs) {
        succeeded &= send_key_sequence(fd, input.input);
        succeeded &= send(fd, static_cast<int32_t>(input.output_index));
      }

      // outputs
      succeeded &= send(fd, static_cast<uint32_t>(context.outputs.size()));
      for (const auto& output : context.outputs)
        succeeded &= send_key_sequence(fd, output);

      // command outputs
      succeeded &= send(fd, static_cast<uint32_t>(context.command_outputs.size()));
      for (const auto& command : context.command_outputs) {
        succeeded &= send_key_sequence(fd, command.output);
        succeeded &= send(fd, static_cast<int32_t>(command.index));
      }
    }
    return succeeded;
  }

  bool send_active_contexts(int fd, const std::vector<int>& indices) {
    auto succeeded = send(fd, static_cast<uint32_t>(indices.size()));
    for (const auto& index : indices)
      succeeded &= send(fd, static_cast<uint32_t>(index));
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
  ::send(m_socket_fd, MessageType::update_configuration);
  return ::send_config(m_socket_fd, config);
}

bool ServerPort::send_active_contexts(const std::vector<int>& indices) {
  ::send(m_socket_fd, MessageType::set_active_contexts);
  return ::send_active_contexts(m_socket_fd, indices);
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
