
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
  m_controls.clear();
}

std::optional<Socket> ControlPort::listen() {
  if (m_host.listen())
    return m_host.listen_socket();
  return { };
}

std::optional<Socket> ControlPort::accept() {
  if (auto connection = m_host.accept(std::chrono::seconds::zero())) {
    const auto socket = connection.socket();
    m_controls.emplace(socket, Control{ std::move(connection) });
    return socket;
  }
  return { };
}

void ControlPort::set_virtual_key_aliases(
    std::vector<std::pair<std::string, Key>> aliases) {
  m_virtual_key_aliases = std::move(aliases);
}

void ControlPort::on_virtual_key_state_changed(Key key, KeyState state) {
  if (is_virtual_key(key)) {
    m_virtual_keys_down[*key - *Key::first_virtual] = (state == KeyState::Down);
    send_virtual_key_toggle_notification(key);
  }
}

void ControlPort::send_virtual_key_toggle_notification(Key key) {
  for (auto& [socket, control] : m_controls)
    if (control.requested_virtual_key_toggle_notification == key) {
      control.requested_virtual_key_toggle_notification = Key::none;
      send_virtual_key_state(control.connection, key);
    }
}

auto ControlPort::get_control(const Connection& connection) -> Control* {
  const auto it = m_controls.find(connection.socket());
  return (it != m_controls.end() ? &it->second : nullptr);
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
  return (is_virtual_key(key) ? key : Key::none);
}

KeyState ControlPort::get_virtual_key_state(Key key) const {
  if (is_virtual_key(key))
    return (m_virtual_keys_down[*key - *Key::first_virtual] ? 
      KeyState::Down : KeyState::Up);
  return KeyState::Not;
}

bool ControlPort::send_virtual_key_state(Connection& connection, Key key, KeyState state) {
  return connection.send_message([&](Serializer& s) {
    s.write(MessageType::virtual_key_state);
    s.write(key);
    s.write(state);
  });
}

bool ControlPort::send_virtual_key_state(Connection& connection, Key key) {
  return send_virtual_key_state(connection, key, get_virtual_key_state(key));
}

void ControlPort::on_request_virtual_key_toggle_notification(
    Connection& connection, Key key) {
  if (key != Key::none) {
    if (auto control = get_control(connection))
      control->requested_virtual_key_toggle_notification = key;
  }
  else {
    send_virtual_key_state(connection, key);
  }
}

void ControlPort::on_set_instance_id(Connection& connection, std::string id) {
  if (auto control = get_control(connection))
    if (control->instance_id != id) {
      disconnect_by_instance_id(id);
      control->instance_id = std::move(id);
    }
}

void ControlPort::disconnect_by_instance_id(const std::string& id) {
  if (id.empty() || id == "0")
    return;

  for (auto it = m_controls.begin(); it != m_controls.end(); ) {
    if (it->second.instance_id == id) {
      it = m_controls.erase(it);
    }
    else {
      ++it;
    }
  }
}

void ControlPort::read_messages(MessageHandler& handler) {
  for (auto it = m_controls.begin(); it != m_controls.end(); ) {
    if (!read_messages(it->second.connection, handler)) {
      it = m_controls.erase(it);
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
        case MessageType::set_instance_id: {
          on_set_instance_id(connection, d.read_string());
          send_virtual_key_state(connection, Key::none);
          break;
        }
        case MessageType::set_config_file: {
          const auto result = handler.on_set_config_file_message(d.read_string());
          send_virtual_key_state(connection, Key::none, 
            (result ? KeyState::Down : KeyState::Up));
          break;
        }
        default: 
          break;
      }
    });
}
