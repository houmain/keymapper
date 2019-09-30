
#include "FocusedWindow.h"
#include "win.h"
#include <array>

class FocusedWindow {
public:
  HWND current() const { return m_current; }
  const std::string& get_class() const { return m_class; }
  const std::string& get_title() const { return m_title; }

  bool update() {
    const auto hwnd = GetForegroundWindow();
    if (hwnd == m_current)
      return false;

    m_current = hwnd;

    auto buffer = std::array<char, 256>();
    GetClassNameA(hwnd, buffer.data(), static_cast<int>(buffer.size()));
    m_class = buffer.data();

    GetWindowTextA(hwnd, buffer.data(), static_cast<int>(buffer.size()));
    m_title = buffer.data();
    return true;
  }

private:
  HWND m_current{ };
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
    auto handle = OpenProcess(PROCESS_VM_READ, FALSE, process_id);
    if (!handle)
      return true;
    CloseHandle(handle);
  }
  return false;
}
