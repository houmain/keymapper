
#include "client/FocusedWindow.h"
#include "common/windows/win.h"
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

class FocusedWindowImpl {
private:
  HWND m_current_window{ };
  std::wstring m_current_title;
  std::string m_class;
  std::string m_title;

public:
  HWND current() const { return m_current_window; }
  const std::string& window_class() const { return m_class; }
  const std::string& window_title() const { return m_title; }

  bool update() {
    const auto hwnd = GetForegroundWindow();
    if (!hwnd)
      return false;

    const auto max_title_length = 1024;
    auto buffer = std::array<wchar_t, max_title_length>();
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
};

FocusedWindow::FocusedWindow()
  : m_impl(std::make_unique<FocusedWindowImpl>()) {
}

FocusedWindow::FocusedWindow(FocusedWindow&& rhs) noexcept = default;
FocusedWindow& FocusedWindow::operator=(FocusedWindow&& rhs) noexcept = default;
FocusedWindow::~FocusedWindow() = default;

bool FocusedWindow::update() {
  return m_impl->update();
}

const std::string& FocusedWindow::window_class() const {
  return m_impl->window_class();
}

const std::string& FocusedWindow::window_title() const {
  return m_impl->window_title();
}

bool FocusedWindow::is_inaccessible() const {
  if (auto hwnd = m_impl->current()) {
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
