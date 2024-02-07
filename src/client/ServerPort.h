#pragma once

#include <memory>
#include "common/Connection.h"
#include "common/MessageType.h"
#include "config/Config.h"

class ServerPort {
private:
  std::unique_ptr<Connection> m_connection;

public:
  ServerPort();
  ServerPort(const ServerPort&) = delete;
  ServerPort& operator=(const ServerPort&) = delete;
  ~ServerPort();

  Connection::Socket socket() const;
  bool initialize();
  bool send_config(const Config& config);
  bool send_active_contexts(const std::vector<int>& indices);
  bool send_validate_state();
  bool send_set_virtual_key_state(Key key, KeyState state);

  struct MessageHandler {
    void (*trigger_action)(int action_index);
    void (*virtual_key_state)(Key key, KeyState state);
  };
  bool read_messages(std::optional<Duration> timeout, 
    const MessageHandler& handler);
};
