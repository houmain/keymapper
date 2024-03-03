#pragma once

#include "runtime/KeyEvent.h"
#include "common/MessageType.h"
#include "common/Connection.h"
#include <memory>
#include <vector>
#include <optional>
#include <array>

class ControlPort {
private:
  std::unique_ptr<Connection> m_connection;
  std::array<bool, *Key::last_virtual - *Key::first_virtual> m_virtual_keys_down{ };
  std::vector<std::pair<std::string, Key>> m_virtual_key_aliases;
  Key m_requested_virtual_key_toggle_notification{ };

  Key get_virtual_key(const std::string_view name) const;
  KeyState get_virtual_key_state(Key key) const;
  bool send_virtual_key_state(Key key);
  void on_request_virtual_key_toggle_notification(Key key);

public:
  Connection::Socket socket() const;
  Connection::Socket listen_socket() const;
  bool initialize();
  bool accept();
  void disconnect();
  void set_virtual_key_aliases(std::vector<std::pair<std::string, Key>> aliases);

  void on_virtual_key_state_changed(Key key, KeyState state);

  struct MessageHandler {
    virtual void on_set_virtual_key_state_message(Key key, KeyState state) = 0;
  };
  bool read_messages(MessageHandler& handler);
};
