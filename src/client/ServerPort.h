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
  bool receive_triggered_action(int timeout_ms, int* triggered_action);
};
