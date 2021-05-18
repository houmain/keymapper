
#include "FocusedWindow.h"
#include "win.h"
#include <array>
#include <cstring>

namespace {
  std::string wide_to_utf8(const std::wstring_view& str) {
    auto result = std::string();
    result.resize(WideCharToMultiByte(CP_UTF8, 0, 
      str.data(), static_cast<int>(str.size()), 
      NULL, 0, 
      NULL, 0));
    WideCharToMultiByte(CP_UTF8, 0, 
      str.data(), static_cast<int>(str.size()), 
      result.data(), static_cast<int>(result.size()),
      NULL, 0);
    return result;
  }
} // namespace

class FocusedWindow {
public:
  HWND current() const { return m_current_window; }
  const std::string& get_class() const { return m_class; }
  const std::string& get_title() const { return m_title; }

  bool update() {
    const auto hwnd = GetForegroundWindow();
    if (!hwnd)
      return false;

    auto buffer = std::array<wchar_t, 256>();
    GetWindowTextW(hwnd, buffer.data(), static_cast<int>(buffer.size()));

    if (hwnd == m_current_window &&
        !lstrcmpW(buffer.data(), m_current_title.c_str()))
      return false;

    m_current_window = hwnd;
    m_current_title = buffer.data();

    GetClassNameW(hwnd, buffer.data(), static_cast<int>(buffer.size()));
    m_class = wide_to_utf8(buffer.data());
    m_title = wide_to_utf8(m_current_title);
    return true;
  }

private:
  HWND m_current_window{ };
  std::wstring m_current_title;
  std::string m_class;
  std::string m_title;
};

void FreeFocusedWindow::operator()(FocusedWindow* window) { delete window; }

FocusedWindowPtr create_focused_window() {
  return FocusedWindowPtr{ new FocusedWindow() };
}

bool update_focused_window(FocusedWindow& window) {
  return window.update();
}

const std::string& get_class(const FocusedWindow& window) {
  return window.get_class();
}

const std::string& get_title(const FocusedWindow& window) {
  return window.get_title();
}

bool is_inaccessible(const FocusedWindow& window) {
  if (auto hwnd = window.current()) {
    auto process_id = DWORD{ };
    GetWindowThreadProcessId(hwnd, &process_id);
    if (process_id) {
      const auto handle = OpenProcess(PROCESS_VM_READ, FALSE, process_id);
      if (!handle)
        return true;
      CloseHandle(handle);
    }
  }
  return false;
}
