#pragma once

#include <memory>
#include <vector>
#include <optional>
#include "common/MessageType.h"
#include "common/Connection.h"

class Stage;

class ClientPort {
private:
  std::unique_ptr<Connection> m_connection;
  std::vector<int> m_active_context_indices;

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

  template<typename F> // void(Deserializer&)
  bool read_messages(int timeout_ms, F&& deserialize) {
    return m_connection && m_connection->read_messages(
      timeout_ms, std::forward<F>(deserialize));
  }
  std::unique_ptr<Stage> read_config(Deserializer& d);
  const std::vector<int>& read_active_contexts(Deserializer& d);
  bool send_triggered_action(int action);
};
