#pragma once

#include "runtime/KeyEvent.h"
#include "common/MessageType.h"
#include "common/Host.h"
#include <memory>
#include <vector>
#include <optional>
#include <array>

class ControlPort {
public:
  ControlPort();
  void reset();
  std::optional<Socket> listen();
  std::optional<Socket> accept();
  void set_virtual_key_aliases(std::vector<std::pair<std::string, Key>> aliases);
  void on_virtual_key_state_changed(Key key, KeyState state);

  struct MessageHandler {
    virtual void on_set_virtual_key_state_message(Key key, KeyState state) = 0;
  };
  void read_messages(MessageHandler& handler);

private:
  struct Request {
    Key key;
    Connection* connection;
  };

  Host m_host;
  std::array<bool, *Key::last_virtual - *Key::first_virtual> m_virtual_keys_down{ };
  std::vector<std::pair<std::string, Key>> m_virtual_key_aliases;
  std::vector<std::unique_ptr<Connection>> m_connections;
  std::vector<Request> m_requested_virtual_key_toggle_notification{ };

  Key get_virtual_key(const std::string_view name) const;
  KeyState get_virtual_key_state(Key key) const;
  bool read_messages(Connection& connection, MessageHandler& handler);
  bool send_virtual_key_state(Connection& connection, Key key);
  void send_virtual_key_toggle_notification(Key key);
  void on_request_virtual_key_toggle_notification(
    Connection& connection, Key key);
  void on_disconnected(Connection& connection);
};
