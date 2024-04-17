
#include "Host.h"
#include <thread>

#if defined(_WIN32)

#include "common/windows/win.h"
#include "common/output.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <afunix.h>
#define close closesocket

Host::Host(std::string ipc_id) 
  : m_ipc_id(std::move(ipc_id)) {

  static struct StaticInitWinSock {
    StaticInitWinSock() {
      auto data = WSADATA{ };
      if (WSAStartup(MAKEWORD(2, 2), &data))
        error("Initializing Winsock 2.2 failed");
    }
    ~StaticInitWinSock() {
      WSACleanup();
    }
  } s;
}

void set_unix_domain_socket_path(const std::string& ipc_id, 
    sockaddr_un& addr, bool unlink) {
  // use non-abstract socket address, since that is not really supported yet
  // https://github.com/microsoft/WSL/issues/4240
  const auto length = sizeof(addr.sun_path) - 1;
  auto buffer = addr.sun_path;
  const auto written = GetTempPathA(length, buffer);
  ::strncpy(&buffer[written], ipc_id.c_str(), length - written);

  if (unlink)
    DeleteFileA(addr.sun_path);
}

void make_non_blocking(Socket socket_fd) {
  auto mode = u_long{ 1 };
  ::ioctlsocket(socket_fd, FIONBIO, &mode);
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

Host::Host(std::string ipc_id) 
  : m_ipc_id(std::move(ipc_id)) {

  ::signal(SIGPIPE, [](int) { });
}

void set_unix_domain_socket_path(const std::string& ipc_id, 
    sockaddr_un& addr, [[maybe_unused]] bool unlink) {

# if defined(__linux)
  // use abstract socket address "a nonportable Linux extension."
  // https://man7.org/linux/man-pages/man7/unix.7.html
  addr.sun_path[0] = '\0';
  ::strncpy(&addr.sun_path[1], ipc_id.c_str(), sizeof(addr.sun_path) - 2);

# else // !defined(__linux)

  const auto length = sizeof(addr.sun_path) - 1;
  std::strncpy(addr.sun_path, "/tmp/", length);
  std::strncat(addr.sun_path, ipc_id.c_str(), length);

  if (unlink)
    ::unlink(addr.sun_path);

# endif // !defined(__linux)
}

void make_non_blocking(Socket socket_fd) {
  auto flags = ::fcntl(socket_fd, F_GETFL, 0);
  ::fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK);
}

#endif // !defined(_WIN32)

Host::~Host() {
  shutdown();
}

bool Host::listen() {
  m_listen_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (m_listen_fd == invalid_socket)
    return false;

  auto addr = sockaddr_un{ };
  addr.sun_family = AF_UNIX;
  set_unix_domain_socket_path(m_ipc_id, addr, true);

  if (::bind(m_listen_fd, reinterpret_cast<sockaddr*>(&addr),
        sizeof(sockaddr_un)) != 0)
    return false;

#if !defined(_WIN32) && !defined(__linux)
  // let unprivileged process connect
  ::chmod(addr.sun_path, 0666);
#endif

  if (::listen(m_listen_fd, 0) != 0)
    return false;

  make_non_blocking(m_listen_fd);
  return true;
}

void Host::shutdown() {
  if (m_listen_fd != invalid_socket) {
    ::close(m_listen_fd);
    auto addr = sockaddr_un{ };
    set_unix_domain_socket_path(m_ipc_id, addr, true);
    m_listen_fd = invalid_socket;
  }
}

Connection Host::accept(std::optional<Duration> timeout) {
  if (!block_until_readable(m_listen_fd, timeout))
    return { };

  auto socket_fd = ::accept(m_listen_fd, nullptr, nullptr);
  if (socket_fd == invalid_socket)
    return { };
  auto connection = Connection(socket_fd);
  
  // send something since accept also unblocks another client's connect
  const auto accept = char{ 1 };
  if (!connection.send(&accept, sizeof(accept)))
    return { };

  make_non_blocking(socket_fd);
  return connection;
}

Connection Host::connect(std::optional<Duration> timeout) {
  auto socket_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (socket_fd == invalid_socket)
    return { };
   
  auto addr = sockaddr_un{ };
  addr.sun_family = AF_UNIX;
  set_unix_domain_socket_path(m_ipc_id, addr, false);

  const auto retry_until_timepoint = (timeout ? 
    std::make_optional(Clock::now() + *timeout) : std::nullopt);

  for (;;) {
    if (::connect(socket_fd, reinterpret_cast<sockaddr*>(&addr),
          sizeof(sockaddr_un)) == 0) {
      auto connection = Connection(socket_fd);
      
      // read message which is sent after accept
      auto accept = char{ 0 };
      if (!connection.recv(&accept, sizeof(accept)) || 
          accept != 1)
        return { };

      make_non_blocking(socket_fd);
      return connection;
    }
    if (retry_until_timepoint && Clock::now() > retry_until_timepoint)
      break;

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  return { };
}
