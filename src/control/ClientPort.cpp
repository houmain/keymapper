
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

bool ClientPort::send_set_instance_id(std::string_view id) {
  return m_connection.send_message([&](Serializer& s) {
    s.write(MessageType::set_instance_id);
    s.write(id);
  });
}

bool ClientPort::send_set_config_file(const std::string& filename) {
  return m_connection.send_message([&](Serializer& s) {
    s.write(MessageType::set_config_file);
    s.write(filename);
  });
}

bool ClientPort::read_virtual_key_state(std::optional<Duration> timeout, 
    std::optional<KeyState>* result) {
  return m_connection.read_messages(timeout,
    [&](Deserializer& d) {
      switch (d.read<MessageType>()) {
        case MessageType::virtual_key_state: {
          [[maybe_unused]] const auto key = d.read<Key>();
          const auto state = d.read<KeyState>();
          result->emplace(state);
          break;
        }
        default: 
          break;
      }
    });
}
