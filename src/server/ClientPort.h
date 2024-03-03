#pragma once

#include "runtime/Stage.h"
#include "common/MessageType.h"
#include "common/Connection.h"
#include <memory>

class ClientPort {
private:
  std::unique_ptr<Connection> m_connection;
  std::vector<int> m_active_context_indices;

  std::unique_ptr<Stage> read_config(Deserializer& d);
  const std::vector<int>& read_active_contexts(Deserializer& d);

public:
  Connection::Socket socket() const;
  Connection::Socket listen_socket() const;
  bool initialize();
  bool accept();
  void disconnect();

  bool send_triggered_action(int action);
  bool send_virtual_key_state(Key key, KeyState state);

  struct MessageHandler {
    virtual void on_configuration_message(std::unique_ptr<Stage> stage) = 0;
    virtual void on_active_contexts_message(const std::vector<int>& context_indices) = 0;
    virtual void on_set_virtual_key_state_message(Key key, KeyState state) = 0;
    virtual void on_validate_state_message() { }
  };
  bool read_messages(MessageHandler& handler, std::optional<Duration> timeout);
};
