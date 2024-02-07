
#include "ClientPort.h"

ClientPort::ClientPort() = default;
ClientPort::~ClientPort() = default;

Connection::Socket ClientPort::socket() const {
  return (m_connection ? m_connection->socket() : Connection::invalid_socket);
}

bool ClientPort::initialize(std::optional<Duration> timeout) {
  auto connection = std::make_unique<Connection>("keymapperctl");
  if (!connection->connect(timeout))
    return false;

  m_connection = std::move(connection);
  return true;
}

bool ClientPort::send_get_virtual_key_state(std::string_view name) {
  return m_connection && m_connection->send_message([&](Serializer& s) {
    s.write(MessageType::get_virtual_key_state);
    s.write(name);
  });
}

bool ClientPort::send_set_virtual_key_state(std::string_view name, KeyState state) {
  return m_connection && m_connection->send_message([&](Serializer& s) {
    s.write(MessageType::set_virtual_key_state);
    s.write(name);
    s.write(state);
  });
}

bool ClientPort::send_request_virtual_key_toggle_notification(std::string_view name) {
  return m_connection && m_connection->send_message([&](Serializer& s) {
    s.write(MessageType::request_virtual_key_toggle_notification);
    s.write(name);
  });
}

std::optional<KeyState> ClientPort::read_virtual_key_state(std::optional<Duration> timeout) {
  auto result = std::optional<KeyState>();
  if (m_connection && m_connection->read_messages(timeout,
    [&](Deserializer& d) {
      switch (d.read<MessageType>()) {
        case MessageType::virtual_key_state: {
          const auto key = d.read<Key>();
          const auto state = d.read<KeyState>();
          result.emplace(state);
        }
        default: break;
      }
    }));
  return result;
}
