#pragma once

#include "common/Host.h"
#include "common/MessageType.h"
#include "config/Config.h"
#include <memory>

class ServerPort {
public:
  ServerPort();
  Socket socket() const { return m_connection.socket(); }
  bool connect();
  void disconnect();
  bool send_config(const Config& config);
  bool send_active_contexts(const std::vector<int>& indices);
  bool send_validate_state();
  bool send_set_virtual_key_state(Key key, KeyState state);
  bool send_request_device_names();

  struct MessageHandler {
    virtual void on_execute_action_message(int action_index) = 0;
    virtual void on_virtual_key_state_message(Key key, KeyState state) = 0;
    virtual void on_device_names_message(std::vector<std::string> devices) = 0;
  };
  bool read_messages(MessageHandler& handler, std::optional<Duration> timeout);

private:
  Host m_host;
  Connection m_connection;
};
