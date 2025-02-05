
#include "output.h"
#include <cstdio>
#include <cstdarg>
#include <array>
#include <string>

bool g_verbose_output = false;
void(*g_show_notification)(const char* message);
void(*g_show_message_box)(const char* title, const char* message);

constexpr const char* current_year() {
  const auto date = __DATE__;
  return &date[7];
}

const auto about_header_str = std::string(
#if __has_include("_version.h")
# include "_version.h"
#endif
  "\n(c) 2019-") + current_year() + " by Albert Kalchmair";

const char* about_header = about_header_str.c_str();
const char* about_footer =
    "All Rights Reserved.\n"
    "This program comes with absolutely no warranty.\n"
    "See the GNU General Public License, version 3 for details.";

extern void show_notification(const char* message);

#if defined(_WIN32)
# include "windows/win.h"
# include <winsock2.h>
#endif

namespace {
  const char* g_message_box_title = "keymapper";

  void vprint(const char* format, va_list args,
      bool notify, bool is_error, bool is_verbose) {

    auto buffer = std::array<char, 2048>();
    std::vsnprintf(buffer.data(), buffer.size(), format, args);

#if defined(_WIN32)
    static const auto s_has_console = [](){
      if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE *stream;
        freopen_s(&stream, "CONOUT$", "w", stdout);
        std::fputc('\n', stdout);
        return true;
      }
      return false;
    }();

# if !defined(NDEBUG)
    OutputDebugStringA(buffer.data());
    OutputDebugStringA("\n");
# endif
#endif

    if (is_error)
      std::fputs("ERROR: ", stdout);
    std::fputs(buffer.data(), stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);

    if (!is_verbose) {
      if (notify) {
        if (g_show_notification)
          g_show_notification(buffer.data());
      }
      else {
        if (g_show_message_box)
          g_show_message_box(g_message_box_title, buffer.data());
      }
    }
  }
} // namespace

void set_message_box_title(const char* title) {
  g_message_box_title = title;
}

void message(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vprint(format, args, false, false, false);
  va_end(args);
}

void notify(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vprint(format, args, true, false, false);
  va_end(args);
}

void error(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vprint(format, args, true, true, false);
  va_end(args);
}

void verbose(const char* format, ...) {
  if (g_verbose_output) {
    va_list args;
    va_start(args, format);
    vprint(format, args, false, false, true);
    va_end(args);
  }
}
