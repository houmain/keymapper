#pragma once

#include <memory>
#include "runtime/Stage.h"
#include "common/MessageType.h"
#include "common/Connection.h"

class ClientPort {
private:
  std::unique_ptr<Connection> m_connection;
  std::vector<int> m_active_context_indices;

  std::unique_ptr<Stage> read_config(Deserializer& d);
  const std::vector<int>& read_active_contexts(Deserializer& d);

public:
  ClientPort();
  ClientPort(const ClientPort&) = delete;
  ClientPort& operator=(const ClientPort&) = delete;
  ~ClientPort();

  Connection::Socket socket() const;
  Connection::Socket listen_socket() const;
  bool initialize();
  bool accept();
  void disconnect();

  bool send_triggered_action(int action);
  bool send_virtual_key_state(Key key, KeyState state);

  struct MessageHandler {
    void (*configuration)(std::unique_ptr<Stage> stage);
    void (*active_contexts)(const std::vector<int>& context_indices);
    void (*set_virtual_key_state)(Key key, KeyState state);
    void (*validate_state)();
  };
  bool read_messages(std::optional<Duration> timeout, 
    const MessageHandler& handler);
};
