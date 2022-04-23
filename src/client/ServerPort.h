#pragma once

#include <vector>

struct Config;

class ServerPort {
private:
  int m_socket_fd{ -1 };

public:
  ServerPort() = default;
  ServerPort(const ServerPort&) = delete;
  ServerPort& operator=(const ServerPort&) = delete;
  ~ServerPort();

  bool initialize(const char* ipc_filename);
  bool send_config(const Config& config);
  bool send_active_contexts(const std::vector<int>& indices);
  bool receive_triggered_action(int timeout_ms, int* action);
};
