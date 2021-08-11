#pragma once

#include <memory>

class Stage;

class ClientPort {
private:
  int m_ipc_fd{ -1 };

public:
  ClientPort() = default;
  ClientPort(const ClientPort&) = delete;
  ClientPort& operator=(const ClientPort&) = delete;
  ~ClientPort();

  bool initialize(const char* fifo_filename);
  std::unique_ptr<Stage> read_config();
  bool receive_updates(Stage& stage);
};
