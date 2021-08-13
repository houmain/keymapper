#pragma once

#include <memory>

class Stage;

class ClientPort {
private:
  int m_socket_fd{ -1 };
  int m_client_fd{ -1 };

public:
  ClientPort() = default;
  ClientPort(const ClientPort&) = delete;
  ClientPort& operator=(const ClientPort&) = delete;
  ~ClientPort();

  bool initialize(const char* ipc_id);
  std::unique_ptr<Stage> read_config();
  bool receive_updates(Stage& stage);
  bool send_triggered_action(int action);
  void disconnect();
};
