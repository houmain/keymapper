#pragma once

#include "common/Connection.h"
#include "common/MessageType.h"
#include "config/Config.h"
#include <memory>

class ServerPort {
private:
  std::unique_ptr<Connection> m_connection;

public:
  Connection::Socket socket() const;
  bool initialize();
  bool send_config(const Config& config);
  bool send_active_contexts(const std::vector<int>& indices);
  bool send_validate_state();
  bool send_set_virtual_key_state(Key key, KeyState state);

  struct MessageHandler {
    virtual void on_execute_action_message(int action_index) = 0;
    virtual void on_virtual_key_state_message(Key key, KeyState state) = 0;
  };
  bool read_messages(MessageHandler& handler, std::optional<Duration> timeout);
};
