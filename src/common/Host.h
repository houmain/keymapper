#pragma once

#include "Connection.h"

class Host {
public:
  explicit Host(std::string ipc_id);
  Host(const Host&) = delete;
  Host& operator=(const Host&) = delete;
  ~Host();
  Socket listen_socket() const { return m_listen_fd; }

  bool listen();
  void shutdown();
  Connection accept(std::optional<Duration> timeout = std::nullopt);
  Connection connect(std::optional<Duration> timeout = std::nullopt);

private:
  const std::string m_ipc_id;
  Socket m_listen_fd{ ~Socket() };
};
