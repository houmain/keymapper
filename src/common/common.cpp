
#include "common.h"
#include <cstdio>
#include <array>

bool g_verbose_output = false;
bool g_output_color = true;

#if defined(_WIN32)

#include "windows/win.h"

namespace {
  void vprint(bool notify, const char* format, va_list args) {
#if defined(NDEBUG)
    static const auto s_has_console = [](){
      if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE *stream;
        freopen_s(&stream, "CONOUT$", "w", stdout);
        std::fputc('\n', stdout);
        return true;
      }
      return false;
    }();
    if (s_has_console) {
      std::vfprintf(stdout, format, args);
      std::fputc('\n', stdout);
      std::fflush(stdout);
      return;
    }
#endif

    auto buffer = std::array<char, 1024>();
    std::vsnprintf(buffer.data(), buffer.size(), format, args);

#if !defined(NDEBUG)
    OutputDebugStringA(buffer.data());
    OutputDebugStringA("\n");
#else
    if (notify)
      MessageBoxA(nullptr, buffer.data(), "Keymapper", 
        MB_ICONWARNING | MB_TOPMOST);
#endif
  }
} // namespace

void error(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vprint(true, format, args);
  va_end(args);
}

void verbose(const char* format, ...) {
  if (g_verbose_output) {
    va_list args;
    va_start(args, format);
    vprint(false, format, args);
    va_end(args);
  }
}

#else // !defined(_WIN32)

#include <unistd.h>
#include <cerrno>
#include <sys/select.h>

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

#endif // !defined(_WIN32)
