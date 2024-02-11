#pragma once

#include <memory>
#include <vector>
#include "common/Connection.h"

struct Config;

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

  template<typename F> // void(Deserializer&)
  bool read_messages(std::optional<Duration> timeout, F&& deserialize) {
    return m_connection && m_connection->read_messages(
      timeout, std::forward<F>(deserialize));
  }
  int read_triggered_action(Deserializer& d);
};
