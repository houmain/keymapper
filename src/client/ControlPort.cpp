
#include "ControlPort.h"
#include "config/get_key_name.h"
#include <algorithm>

Connection::Socket ControlPort::socket() const {
  return (m_connection ? m_connection->socket() : Connection::invalid_socket);
}

Connection::Socket ControlPort::listen_socket() const {
  return (m_connection ? m_connection->listen_socket() : Connection::invalid_socket);
}

bool ControlPort::initialize() {
  auto connection = std::make_unique<Connection>("keymapperctl");
  if (!connection->listen())
    return false;

  m_connection = std::move(connection);
  return true;
}

bool ControlPort::accept() {
  return (m_connection && m_connection->accept());
}

void ControlPort::disconnect() {
  if (m_connection)
    m_connection->disconnect();
}

void ControlPort::set_virtual_key_aliases(
    std::vector<std::pair<std::string, Key>> aliases) {
  m_virtual_key_aliases = std::move(aliases);
}

void ControlPort::on_virtual_key_state_changed(Key key, KeyState state) {
  if (is_actual_virtual_key(key)) {
    m_virtual_keys_down[*key - *Key::first_virtual] = (state == KeyState::Down);

    if (m_requested_virtual_key_toggle_notification == key)
      send_virtual_key_state(key);
  }
}

Key ControlPort::get_virtual_key(const std::string_view name) const {
  auto key = get_key_by_name(name);
  if (key == Key::none) {
    const auto& aliases = m_virtual_key_aliases;
    const auto it = std::find_if(aliases.begin(), aliases.end(),
      [&](const auto& pair) { return pair.first == name; });
    if (it != aliases.end())
      key = it->second;
  }
  return (is_actual_virtual_key(key) ? key : Key::none);
}

KeyState ControlPort::get_virtual_key_state(Key key) const {
  if (is_actual_virtual_key(key))
    return (m_virtual_keys_down[*key - *Key::first_virtual] ? KeyState::Down : KeyState::Up);
  return KeyState::Not;
}

bool ControlPort::send_virtual_key_state(Key key) {
  return m_connection && m_connection->send_message([&](Serializer& s) {
    s.write(MessageType::virtual_key_state);
    s.write(key);
    s.write(get_virtual_key_state(key));
  });
}

void ControlPort::on_request_virtual_key_toggle_notification(Key key) {
  m_requested_virtual_key_toggle_notification = key;
}

bool ControlPort::read_messages(MessageHandler& handler) {
  return m_connection && m_connection->read_messages(Duration::zero(), 
    [&](Deserializer& d) {
      switch (d.read<MessageType>()) {
        case MessageType::get_virtual_key_state: {
          send_virtual_key_state(get_virtual_key(d.read_string()));
          break;
        }
        case MessageType::set_virtual_key_state: {
          const auto key = get_virtual_key(d.read_string());
          const auto state = d.read<KeyState>();
          handler.on_set_virtual_key_state_message(key, state);
          send_virtual_key_state(key);
          break;
        }
        case MessageType::request_virtual_key_toggle_notification: {
          on_request_virtual_key_toggle_notification(get_virtual_key(d.read_string()));
          break;
        }
        default: break;
      }
    });
}
