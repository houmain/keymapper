
#include "ClientPort.h"
#include "runtime/Stage.h"
#include "../common.h"
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <cerrno>

namespace {
  bool read_key_sequence(int fd, KeySequence* sequence) {
    sequence->clear();

    auto size = uint8_t{ };
    if (!read(fd, &size))
      return false;

    auto event = KeyEvent{ };
    for (auto i = 0; i < size; ++i) {
      if (!read(fd, &event.key) ||
          !read(fd, &event.state))
        return false;
      sequence->push_back(event);
    }
    return true;
  }

  std::unique_ptr<Stage> read_config(int fd) {
    // receive contexts
    auto contexts = std::vector<Stage::Context>();
    auto count = uint32_t{ };
    if (!read(fd, &count))
      return nullptr;
    contexts.resize(count);
    for (auto& context : contexts) {
      // inputs
      if (!read(fd, &count))
        return nullptr;
      context.inputs.resize(count);
      for (auto& input : context.inputs) {
        if (!read_key_sequence(fd, &input.input))
          return nullptr;
        if (!read(fd, &input.output_index))
          return nullptr;
      }

      // outputs
      if (!read(fd, &count))
        return nullptr;
      context.outputs.resize(count);
      for (auto& output : context.outputs)
        if (!read_key_sequence(fd, &output))
          return nullptr;

      // command outputs
      if (!read(fd, &count))
        return nullptr;
      context.command_outputs.resize(count);
      for (auto& command : context.command_outputs) {
        if (!read_key_sequence(fd, &command.output))
          return nullptr;
        if (!read(fd, &command.index))
          return nullptr;
      }
    }
    return std::make_unique<Stage>(std::move(contexts));
  }

  bool read_active_contexts(int fd, std::vector<int>& indices) {
    auto count = uint32_t{ };
    if (!read(fd, &count))
      return false;
    indices.clear();
    for (auto i = 0u; i < count; ++i) {
      auto index = uint32_t{ };
      if (!read(fd, &index))
        return false;
      indices.push_back(index);
    }
    return true;
  }
} // namespace

ClientPort::~ClientPort() {
  if (m_socket_fd >= 0)
    ::close(m_socket_fd);
}

bool ClientPort::initialize(const char* ipc_id) {
  m_socket_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (m_socket_fd < 0)
    return false;

  auto addr = sockaddr_un{ };
  addr.sun_family = AF_UNIX;
  ::strncpy(&addr.sun_path[1], ipc_id, sizeof(addr.sun_path) - 2);
  if (::bind(m_socket_fd, reinterpret_cast<sockaddr*>(&addr),
      sizeof(sockaddr_un)) != 0)
    return false;

  if (::listen(m_socket_fd, 0) != 0)
    return false;

  return true;
}

std::unique_ptr<Stage> ClientPort::receive_config() {
  m_client_fd = ::accept(m_socket_fd, nullptr, nullptr);
  if (m_client_fd < 0)
    return nullptr;

  auto message_type = MessageType{ };
  ::read(m_client_fd, &message_type);
  if (message_type == MessageType::update_configuration)
    return ::read_config(m_client_fd);

  return nullptr;
}

bool ClientPort::receive_updates(std::unique_ptr<Stage>& stage) {
  const auto timeout_ms = 0;
  if (!select(m_client_fd, timeout_ms))
    return true;

  auto message_type = MessageType{ };
  ::read(m_client_fd, &message_type);

  if (message_type == MessageType::update_configuration) {
    if (auto new_stage = ::read_config(m_client_fd)) {
      stage = std::move(new_stage);
      return true;
    }
  }
  else if (message_type == MessageType::set_active_contexts) {
    if (!::read_active_contexts(m_client_fd, m_active_context_indices))
      return false;
    stage->set_active_contexts(m_active_context_indices);
    return true;
  }
  return false;
}

bool ClientPort::send_triggered_action(int action) {
  return send(m_client_fd, static_cast<uint32_t>(action));
}

void ClientPort::disconnect() {
  if (m_client_fd >= 0) {
    ::close(m_client_fd);
    m_client_fd = -1;
  }
}
