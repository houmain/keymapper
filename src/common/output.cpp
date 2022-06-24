
#include "output.h"
#include <cstdio>
#include <cstdarg>
#include <array>

bool g_verbose_output = false;

#if defined(_WIN32)

#include "windows/win.h"
#include <winsock2.h>

extern void show_notification(const char* message);

namespace {
  void vprint(bool notify, const char* format, va_list args) {
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
    }

    auto buffer = std::array<char, 1024>();
    std::vsnprintf(buffer.data(), buffer.size(), format, args);
    if (notify)
      show_notification(buffer.data());

#if !defined(NDEBUG)
    OutputDebugStringA(buffer.data());
    OutputDebugStringA("\n");
#endif
  }
} // namespace

void message(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vprint(true, format, args);
  va_end(args);
}

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

void message(const char* format, ...) {
  va_list args;
  va_start(args, format);
  std::vfprintf(stdout, format, args);
  va_end(args);
  std::fputc('\n', stdout);
  std::fflush(stdout);
}

void error(const char* format, ...) {
  va_list args;
  va_start(args, format);
  std::fprintf(stderr, "ERROR: ");
  std::vfprintf(stderr, format, args);
  va_end(args);
  std::fputc('\n', stderr);
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

#endif // !defined(_WIN32)
