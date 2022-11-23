
#include "Connection.h"
#include <thread>

const auto ipc_id = "keymapper";
const Connection::Socket Connection::invalid_socket = ~Connection::Socket{ };

#if defined(_WIN32)

#include "common/windows/win.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <afunix.h>
#define close closesocket

Connection::Connection() {
  static struct StaticInitWinSock {
    StaticInitWinSock() {
      auto data = WSADATA{ };
      WSAStartup(MAKEWORD(2, 2), &data);
    }
    ~StaticInitWinSock() {
      WSACleanup();
    }
  } s;
}

void set_unix_domain_socket_path(sockaddr_un& addr, bool unlink) {
  // use non-abstract socket address, since that is not really supported yet
  // https://github.com/microsoft/WSL/issues/4240
  const auto length = sizeof(addr.sun_path) - 1;
  auto buffer = addr.sun_path;
  const auto written = GetTempPathA(length, buffer);
  ::strncpy(&buffer[written], ipc_id, length - written);

  if (unlink)
    DeleteFileA(addr.sun_path);
}

#else // !defined(_WIN32)

#include <csignal>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <cerrno>

Connection::Connection() = default;

void set_unix_domain_socket_path(sockaddr_un& addr, bool) {
  // use abstract socket address
  addr.sun_path[0] = '\0';
  ::strncpy(&addr.sun_path[1], ipc_id, sizeof(addr.sun_path) - 2);
}

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

Connection::~Connection() {
  if (m_socket_fd != invalid_socket)
    ::close(m_socket_fd);

  if (m_listen_fd != invalid_socket) {
    ::close(m_listen_fd);
    auto addr = sockaddr_un{ };
    set_unix_domain_socket_path(addr, true);
  }
}

bool Connection::listen() {
  m_listen_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (m_listen_fd == invalid_socket)
    return false;

  auto addr = sockaddr_un{ };
  addr.sun_family = AF_UNIX;
  set_unix_domain_socket_path(addr, true);

  if (::bind(m_listen_fd, reinterpret_cast<sockaddr*>(&addr),
        sizeof(sockaddr_un)) != 0)
    return false;

  if (::listen(m_listen_fd, 0) != 0)
    return false;
  return true;
}

bool Connection::accept() {
  m_socket_fd = ::accept(m_listen_fd, nullptr, nullptr);
  if (m_socket_fd == invalid_socket)
    return false;
  make_non_blocking();
  return true;
}

bool Connection::connect() {
#if !defined(_WIN32)
  ::signal(SIGPIPE, [](int) { });
#endif

  m_socket_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (m_socket_fd == invalid_socket)
    return false;
   
  auto addr = sockaddr_un{ };
  addr.sun_family = AF_UNIX;
  set_unix_domain_socket_path(addr, false);

  for (;;) {
    if (::connect(m_socket_fd, reinterpret_cast<sockaddr*>(&addr),
          sizeof(sockaddr_un)) == 0) {
      make_non_blocking();
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
}

void Connection::make_non_blocking() {
#if !defined(_WIN32)
  auto flags = ::fcntl(m_socket_fd, F_GETFL, 0);
  ::fcntl(m_socket_fd, F_SETFL, flags | O_NONBLOCK);
#else
  auto mode = u_long{ 1 };
  ::ioctlsocket(m_socket_fd, FIONBIO, &mode);
#endif
}

void Connection::disconnect() {
  if (m_socket_fd != invalid_socket) {
    ::close(m_socket_fd);
    m_socket_fd = invalid_socket;
  }
  m_deserializer.buffer.clear();
}

bool Connection::wait_for_message(std::optional<Duration> timeout) {
  auto read_set = fd_set{ };
  for (;;) {
    FD_ZERO(&read_set);
    FD_SET(m_socket_fd, &read_set);
    auto timeoutval = (timeout ? to_timeval(timeout.value()) : timeval{ });
    const auto result = ::select(static_cast<int>(m_socket_fd) + 1,
      &read_set, nullptr, nullptr, (timeout ? &timeoutval : nullptr));
    if (result == -1 && errno == EINTR)
      continue;
    return (result >= 0);
  }
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
