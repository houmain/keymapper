
#include "FocusedWindowImpl.h"
#include "common/output.h"

using MakeFocusedWindowSystem = std::unique_ptr<FocusedWindowSystem>(FocusedWindowData*);
MakeFocusedWindowSystem make_focused_window_x11;
MakeFocusedWindowSystem make_focused_window_dbus;
MakeFocusedWindowSystem make_focused_window_wlroots;
MakeFocusedWindowSystem make_focused_window_carbon;

bool FocusedWindowImpl::initialize() {

  const auto systems = std::initializer_list<std::pair<const char*, MakeFocusedWindowSystem*>>{
#if defined(ENABLE_X11)
      { "X11", &make_focused_window_x11 },
#endif
#if defined(ENABLE_DBUS)
      { "D-BUS", &make_focused_window_dbus },
#endif
#if defined(ENABLE_WAYLAND)
      { "wlroots", &make_focused_window_wlroots },
#endif
#if defined(ENABLE_CARBON)
      { "Carbon", &make_focused_window_carbon },
#endif
    };

  for (auto [name, make_system] : systems) {
    auto system = make_system(this);
    verbose("  %s support: %s", name, (system ? "initialized" : "not available"));
    if (system)
      m_systems.push_back(std::move(system));
  }
  return true;
}

void FocusedWindowImpl::shutdown() {
  m_systems.clear();
}

bool FocusedWindowImpl::update() {
  for (auto& system : m_systems)
    if (system->update())
      return true;
  return false;
}

//-------------------------------------------------------------------------

FocusedWindow::FocusedWindow()
  : m_impl(std::make_unique<FocusedWindowImpl>()) {
}
FocusedWindow::FocusedWindow(FocusedWindow&& rhs) noexcept = default;
FocusedWindow& FocusedWindow::operator=(FocusedWindow&& rhs) noexcept = default;
FocusedWindow::~FocusedWindow() = default;

bool FocusedWindow::initialize() {
  return m_impl->initialize();
}

void FocusedWindow::shutdown() {
  m_impl->shutdown();
}

bool FocusedWindow::update() {
  return m_impl->update();
}

const std::string& FocusedWindow::window_class() const {
  return m_impl->window_class;
}

const std::string& FocusedWindow::window_title() const {
  return m_impl->window_title;
}

const std::string& FocusedWindow::window_path() const {
  return m_impl->window_path;
}

bool FocusedWindow::is_inaccessible() const {
  return true;
}

//-------------------------------------------------------------------------

std::string get_process_path_by_pid(int pid) {
  char path[32];
  std::snprintf(path, sizeof(path), "/proc/%d/exe", pid);
  const auto resolved = ::realpath(path, nullptr);
  if (!resolved)
    return { };
  auto result = std::string(resolved);
  ::free(resolved);
  return result;
}
