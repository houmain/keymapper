
#include "common.h"
#include <cstdio>
#include <unistd.h>
#include <cerrno>
#include <sys/select.h>

bool g_verbose_output = false;

void error(const char* format, ...) {
  std::fputs("\033[1;31m", stderr);
  va_list args;
  va_start(args, format);
  std::vfprintf(stderr, format, args);
  va_end(args);
  std::fputc('\n', stderr);
  std::fputs("\033[0m", stderr);
}

void verbose(const char* format, ...) {
  if (g_verbose_output) {
    va_list args;
    va_start(args, format);
    std::vfprintf(stdout, format, args);
    va_end(args);
    std::fputc('\n', stdout);
    std::fflush(stdout);
  }
}

bool write_all(int fd, const char* buffer, size_t length) {
  while (length != 0) {
    auto ret = ::write(fd, buffer, length);
    if (ret == -1 && errno == EINTR)
      continue;
    if (ret <= 0)
      return false;
    length -= static_cast<size_t>(ret);
    buffer += ret;
  }
  return true;
}

bool select(int fd, timeval* timeout) {
  auto set = fd_set{ };
  FD_ZERO(&set);
  FD_SET(fd, &set);
  ::select(fd + 1, &set, nullptr, nullptr, timeout);
  return (FD_ISSET(fd, &set) != 0);
}

bool read_all(int fd, char* buffer, size_t length) {
  while (length != 0) {
    auto ret = ::read(fd, buffer, length);
    if (ret == -1 && errno == EINTR)
      continue;
    if (ret <= 0)
      return false;
    length -= static_cast<size_t>(ret);
    buffer += ret;
  }
  return true;
}
