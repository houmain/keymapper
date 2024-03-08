
#include "ControlPort.h"
#include "config/get_key_name.h"
#include <algorithm>

ControlPort::ControlPort() 
  : m_host("keymapperctl") {
}

void ControlPort::reset() {
  m_host.shutdown();
  m_virtual_keys_down = { };
  m_virtual_key_aliases = { };
  m_connections.clear();
  m_requested_virtual_key_toggle_notification.clear();
}

std::optional<Socket> ControlPort::listen() {
  if (m_host.listen())
    return m_host.listen_socket();
  return { };
}

std::optional<Socket> ControlPort::accept() {
  if (auto connection = m_host.accept(std::chrono::seconds::zero())) {
    auto socket = connection.socket();
    m_connections.push_back(std::make_unique<Connection>(std::move(connection)));
    return socket;
  }
  return { };
}

void ControlPort::set_virtual_key_aliases(
    std::vector<std::pair<std::string, Key>> aliases) {
  m_virtual_key_aliases = std::move(aliases);
}

void ControlPort::on_virtual_key_state_changed(Key key, KeyState state) {
  if (is_actual_virtual_key(key)) {
    m_virtual_keys_down[*key - *Key::first_virtual] = (state == KeyState::Down);
    send_virtual_key_toggle_notification(key);
  }
}

void ControlPort::send_virtual_key_toggle_notification(Key key) {
  auto& requests = m_requested_virtual_key_toggle_notification;
  for (auto it = requests.begin(); it != requests.end(); ) {
    const auto& [request_key, connection] = *it;
    if (request_key == key) {
      send_virtual_key_state(*connection, key);
      it = requests.erase(it);
    }
    else {
      ++it;
    }
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
    return (m_virtual_keys_down[*key - *Key::first_virtual] ? 
      KeyState::Down : KeyState::Up);
  return KeyState::Not;
}

bool ControlPort::send_virtual_key_state(Connection& connection, Key key) {
  return connection.send_message([&](Serializer& s) {
    s.write(MessageType::virtual_key_state);
    s.write(key);
    s.write(get_virtual_key_state(key));
  });
}

void ControlPort::on_request_virtual_key_toggle_notification(
    Connection& connection, Key key) {
  if (key != Key::none) {
    m_requested_virtual_key_toggle_notification.push_back(
      { key, &connection });
  }
  else {
    send_virtual_key_state(connection, key);
  }
}

void ControlPort::on_disconnected(Connection& connection) {
  auto& requests = m_requested_virtual_key_toggle_notification;
  requests.erase(std::remove_if(requests.begin(), requests.end(), 
    [&](const Request& request) { return request.connection == &connection; }),
    requests.end());
}

void ControlPort::read_messages(MessageHandler& handler) {
  for (auto it = m_connections.begin(); it != m_connections.end(); ) {
    if (!read_messages(**it, handler)) {
      on_disconnected(**it);
      it = m_connections.erase(it);
    }
    else {
      ++it;
    }
  }
}

bool ControlPort::read_messages(Connection& connection, 
    MessageHandler& handler) {
  return connection.read_messages(Duration::zero(), 
    [&](Deserializer& d) {
      switch (d.read<MessageType>()) {
        case MessageType::get_virtual_key_state: {
          send_virtual_key_state(connection, 
            get_virtual_key(d.read_string()));
          break;
        }
        case MessageType::set_virtual_key_state: {
          const auto key = get_virtual_key(d.read_string());
          const auto state = d.read<KeyState>();
          handler.on_set_virtual_key_state_message(key, state);
          send_virtual_key_state(connection, key);
          break;
        }
        case MessageType::request_virtual_key_toggle_notification: {
          on_request_virtual_key_toggle_notification(connection,
            get_virtual_key(d.read_string()));
          break;
        }
        default: break;
      }
    });
}
