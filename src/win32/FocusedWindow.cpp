
#include "FocusedWindow.h"
#include "win.h"
#include <array>

class FocusedWindow {
public:
  const std::string& get_class() const { return m_focused_window_class; }
  const std::string& get_title() const { return m_focused_window_title; }

  bool update() {
    const auto window = GetForegroundWindow();
    if (window == m_focused_window)
      return false;

    m_focused_window = window;

    auto buffer = std::array<char, 256>();
    GetClassNameA(window, buffer.data(), buffer.size());
    m_focused_window_class = buffer.data();

    GetWindowTextA(window, buffer.data(), buffer.size());
    m_focused_window_title = buffer.data();
    return true;
  }

private:
  HWND m_focused_window{ };
  std::string m_focused_window_class;
  std::string m_focused_window_title;
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

