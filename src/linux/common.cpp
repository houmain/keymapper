
#include "common.h"
#include <cstdio>
#include <unistd.h>
#include <cerrno>
#include <sys/select.h>

bool g_verbose_output = false;
bool g_output_color = true;

void error(const char* format, ...) {
  if (g_output_color)
      std::fputs("\033[1;31m", stderr);
  va_list args;
  va_start(args, format);
  std::vfprintf(stderr, format, args);
  va_end(args);
  std::fputc('\n', stderr);
  if (g_output_color)
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

bool select(int fd, int timeout_ms) {
  auto set = fd_set{ };
  for (;;) {
    FD_ZERO(&set);
    FD_SET(fd, &set);
    auto timeout = timeval{ 0, timeout_ms * 1000 };
    auto ret = ::select(fd + 1, &set, nullptr, nullptr, &timeout);
    if (ret == -1 && errno == EINTR)
      continue;
    if (ret == 0)
      return false;
    return (FD_ISSET(fd, &set) != 0);
  }
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
