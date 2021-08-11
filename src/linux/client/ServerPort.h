#pragma once

struct Config;

class ServerPort {
private:
  int m_ipc_fd{ -1 };

public:
  ServerPort() = default;
  ServerPort(const ServerPort&) = delete;
  ServerPort& operator=(const ServerPort&) = delete;
  ~ServerPort();

  bool initialize(const char* fifo_filename);
  bool send_config(const Config& config);
  bool send_active_override_set(int index);
  bool is_pipe_broken();
};
