#pragma once

#include <memory>
#include <vector>

class Stage;

class ClientPort {
private:
  int m_socket_fd{ -1 };
  int m_client_fd{ -1 };
  std::vector<int> m_active_context_indices;

public:
  ClientPort() = default;
  ClientPort(const ClientPort&) = delete;
  ClientPort& operator=(const ClientPort&) = delete;
  ~ClientPort();

  bool initialize(const char* ipc_id);
  std::unique_ptr<Stage> receive_config();
  bool receive_updates(std::unique_ptr<Stage>& stage);
  bool send_triggered_action(int action);
  void disconnect();
};