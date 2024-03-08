
#include "ClientPort.h"

ClientPort::ClientPort() 
  : m_host("keymapperctl") {
}

bool ClientPort::connect(std::optional<Duration> timeout) {
  m_connection = m_host.connect(timeout);
  return static_cast<bool>(m_connection);
}

bool ClientPort::send_get_virtual_key_state(std::string_view name) {
  return m_connection.send_message([&](Serializer& s) {
    s.write(MessageType::get_virtual_key_state);
    s.write(name);
  });
}

bool ClientPort::send_set_virtual_key_state(std::string_view name, KeyState state) {
  return m_connection.send_message([&](Serializer& s) {
    s.write(MessageType::set_virtual_key_state);
    s.write(name);
    s.write(state);
  });
}

bool ClientPort::send_request_virtual_key_toggle_notification(std::string_view name) {
  return m_connection.send_message([&](Serializer& s) {
    s.write(MessageType::request_virtual_key_toggle_notification);
    s.write(name);
  });
}

std::optional<KeyState> ClientPort::read_virtual_key_state(std::optional<Duration> timeout) {
  auto result = std::optional<KeyState>();
  m_connection.read_messages(timeout,
    [&](Deserializer& d) {
      switch (d.read<MessageType>()) {
        case MessageType::virtual_key_state: {
          const auto key = d.read<Key>();
          const auto state = d.read<KeyState>();
          result.emplace(state);
        }
        default: break;
      }
    });
  return result;
}
