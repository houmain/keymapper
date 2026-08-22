
#include "Connection.h"
#include <algorithm>

#if defined(_WIN32)

#include "common/windows/win.h"
#include <winsock2.h>
#define close closesocket

#else // !defined(_WIN32)

#include <utility>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>

#endif // !defined(_WIN32)

timeval to_timeval(const Duration& duration) {
  using namespace std::chrono;
  if (duration < Duration::zero())
    return { };
  const auto sec = duration_cast<seconds>(duration);
  return {
    static_cast<decltype(timeval::tv_sec)>(sec.count()),
    static_cast<decltype(timeval::tv_usec)>(
      duration_cast<microseconds>(duration - sec).count())
  };
}

bool block_until_readable(const Socket* sockets, size_t socket_count,
    std::optional<Duration> timeout) {
  auto read_set = fd_set{ };
  for (;;) {
    FD_ZERO(&read_set);
    auto max_socket_fd = Socket{ };
    auto has_socket = false;
    for (auto i = size_t{ }; i < socket_count; ++i) {
      const auto socket = sockets[i];
      if (socket == invalid_socket)
        continue;
      FD_SET(socket, &read_set);
      max_socket_fd = std::max(max_socket_fd, socket);
      has_socket = true;
    }
    if (!has_socket)
      return false;
    auto timeoutval = (timeout ? to_timeval(timeout.value()) : timeval{ });
    const auto result = ::select(static_cast<int>(max_socket_fd) + 1,
      &read_set, nullptr, nullptr, (timeout ? &timeoutval : nullptr));
    if (result == -1 && errno == EINTR)
      continue;
    return (result >= 0);
  }
}

bool block_until_readable(Socket socket_fd, std::optional<Duration> timeout) {
  return block_until_readable(&socket_fd, 1, timeout);
}

void Connection::add_select_socket(Socket socket) {
  if (socket != invalid_socket &&
      std::find(s_select_sockets.begin(), s_select_sockets.end(), socket) ==
        s_select_sockets.end())
    s_select_sockets.push_back(socket);
}

void Connection::remove_select_socket(Socket socket) {
  s_select_sockets.erase(
    std::remove(s_select_sockets.begin(), s_select_sockets.end(), socket),
    s_select_sockets.end());
}

bool Connection::wait_for_messages(std::optional<Duration> timeout) {
  return block_until_readable(
    s_select_sockets.data(), s_select_sockets.size(), timeout);
}

Connection::Connection(Socket socket) 
  : m_socket_fd(socket) {
  add_select_socket(socket);
}

Connection::Connection(Connection&& rhs) noexcept
  : m_socket_fd(std::exchange(rhs.m_socket_fd, invalid_socket)),
    m_serializer(std::move(rhs.m_serializer)),
    m_deserializer(std::move(rhs.m_deserializer)) {
}

Connection& Connection::operator=(Connection&& rhs) noexcept {
  auto tmp = std::move(rhs);
  std::swap(m_socket_fd, tmp.m_socket_fd);
  std::swap(m_serializer, tmp.m_serializer);
  std::swap(m_deserializer, tmp.m_deserializer);
  return *this;
}

Connection::~Connection() {
  disconnect();
}

void Connection::disconnect() {
  if (m_socket_fd != invalid_socket) {
    remove_select_socket(m_socket_fd);
    ::close(m_socket_fd);
    m_socket_fd = invalid_socket;
  }
  m_serializer.buffer.clear();
  m_deserializer.buffer.clear();
}

bool Connection::send(const char* buffer, size_t length) {
  while (length != 0) {
    const auto result = ::send(m_socket_fd, buffer,
      static_cast<int>(length), 0);
    if (result == -1 && (errno == EINTR || errno == EWOULDBLOCK))
      continue;
    if (result <= 0)
      return false;
    length -= static_cast<size_t>(result);
    buffer += result;
  }
  return true;
}

int Connection::recv(char* buffer, size_t length) {
  auto read = 0;
  while (length != 0) {
    const auto result = ::recv(m_socket_fd, buffer,
      static_cast<int>(length), 0);
#if defined(_WIN32)
    if (result == -1 && WSAGetLastError() == WSAEWOULDBLOCK)
      break;
#else
    if (result == -1 && errno == EINTR)
      continue;
    if (result == -1 && errno == EWOULDBLOCK)
      break;
#endif
    if (result <= 0)
      return -1;
    length -= static_cast<size_t>(result);
    buffer += result;
    read += static_cast<int>(result);
  }
  return read;
}

bool Connection::recv(std::vector<char>& buffer) {
  const auto buffer_grow_size = 1024;
  auto pos = buffer.size();
  for (;;) {
    if (pos == buffer.size())
      buffer.resize(buffer.size() + buffer_grow_size);
    const auto result = recv(buffer.data() + pos, buffer.size() - pos);
    if (result < 0)
      return false;
    if (result == 0)
      break;
    pos += static_cast<size_t>(result);
  }
  buffer.resize(pos);
  return true;
}
