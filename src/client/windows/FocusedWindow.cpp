
#include "client/FocusedWindow.h"
#include "common/windows/win.h"
#include <psapi.h>
#include <array>
#include <cstring>

class FocusedWindowImpl {
private:
  HWND m_current_window{ };
  std::wstring m_current_title;
  std::string m_class;
  std::string m_title;
  std::string m_path;

public:
  HWND current() const { return m_current_window; }
  const std::string& window_class() const { return m_class; }
  const std::string& window_title() const { return m_title; }
  const std::string& window_path() const { return m_path; }

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

#if (PSAPI_VERSION >= 2)
    m_path.clear();
    auto pid = DWORD{ };
    auto buffer_size = DWORD{ buffer.size() };
    if (GetWindowThreadProcessId(hwnd, &pid))
      if (const auto handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid)) {
        if (QueryFullProcessImageNameW(handle, 0, buffer.data(), &buffer_size))
          m_path = wide_to_utf8(buffer.data());
        CloseHandle(handle);
      }
#endif

    return true;
  }
};

FocusedWindow::FocusedWindow()
  : m_impl(std::make_unique<FocusedWindowImpl>()) {
}

FocusedWindow::FocusedWindow(FocusedWindow&& rhs) noexcept = default;
FocusedWindow& FocusedWindow::operator=(FocusedWindow&& rhs) noexcept = default;
FocusedWindow::~FocusedWindow() = default;

bool FocusedWindow::initialize() {
  return true;
}

void FocusedWindow::shutdown() {
}

bool FocusedWindow::update() {
  return m_impl->update();
}

const std::string& FocusedWindow::window_class() const {
  return m_impl->window_class();
}

const std::string& FocusedWindow::window_title() const {
  return m_impl->window_title();
}

const std::string& FocusedWindow::window_path() const {
  return m_impl->window_path();
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
