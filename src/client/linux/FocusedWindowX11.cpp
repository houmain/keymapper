
#if defined(ENABLE_X11)

#include "FocusedWindowImpl.h"
# include <X11/X.h>
# include <X11/Xlib.h>
# include <X11/Xatom.h>
# include <X11/Xutil.h>
# include <X11/Xos.h>

class FocusedWindowX11 : public FocusedWindowSystem {
private:
  FocusedWindowData& m_data;
  Display* m_display{ };
  Window m_root_window{ };
  Atom m_net_active_window_atom{ };
  Atom m_net_wm_name_atom{ };
  Atom m_net_wm_pid_atom{ };
  Atom m_utf8_string_atom{ };
  Window m_focused_window{ };

public:
  explicit FocusedWindowX11(FocusedWindowData* data)
    : m_data(*data) {
  }

  FocusedWindowX11(const FocusedWindowX11&) = delete;
  FocusedWindowX11& operator=(const FocusedWindowX11&) = delete;

  ~FocusedWindowX11() {
    if (m_display)
      XCloseDisplay(m_display);
  }

  bool initialize() {
    m_display = XOpenDisplay(nullptr);
    if (!m_display)
      return false;

    m_root_window = XRootWindow(m_display, 0);
    m_net_active_window_atom = XInternAtom(m_display, "_NET_ACTIVE_WINDOW", False);
    m_net_wm_name_atom = XInternAtom(m_display, "_NET_WM_NAME", False);
    m_net_wm_pid_atom = XInternAtom(m_display, "_NET_WM_PID", False);
    m_utf8_string_atom = XInternAtom(m_display, "UTF8_STRING", False);
    XSetErrorHandler([](Display*, XErrorEvent*) { return 0; });
    return true;
  }

  bool update() override {
    const auto window = get_focused_window();
    auto window_title = get_window_title(window);
    if (window == m_focused_window &&
        window_title == m_data.window_title)
      return false;

    // window handles can become invalid any time
    auto window_class = get_window_class(window);
    if (window_class.empty() || window_title.empty())
      return false;

    m_focused_window = window;
    m_data.window_class = std::move(window_class);
    m_data.window_title = std::move(window_title);
    m_data.window_path = get_window_path(window);
    return true;
  }

private:
  Window get_focused_window() {
    auto type = Atom{ };
    auto format = 0;
    auto length = 0ul;
    auto rest = 0ul;
    auto data = std::add_pointer_t<unsigned char>{ };
    if (XGetWindowProperty(m_display, m_root_window, m_net_active_window_atom,
          0L, sizeof(Window), False, XA_WINDOW, &type, &format,
          &length, &rest, &data) == Success &&
        data) {
      auto result = *reinterpret_cast<Window*>(data);
      XFree(data);
      return result;
    }
    return { };
  }

  std::string get_window_class(Window window) {
    auto ch = XClassHint{ };
    if (window &&
        XGetClassHint(m_display, window, &ch) != 0) {
      auto result = std::string(ch.res_name);
      XFree(ch.res_name);
      XFree(ch.res_class);
      return result;
    }
    return { };
  }

  std::string get_window_title(Window window) {
    auto type = Atom{ };
    auto format = 0;
    auto length = 0ul;
    auto rest = 0ul;
    auto data = std::add_pointer_t<unsigned char>{ };
    if (window &&
        XGetWindowProperty(m_display, window, m_net_wm_name_atom, 0, 1024,
          False, m_utf8_string_atom, &type, &format, &length,
          &rest, &data) == Success &&
        data) {
      auto result = std::string(reinterpret_cast<const char*>(data));
      XFree(data);
      return result;
    }
    return { };
  }

  std::string get_window_path(Window window) {
    auto type = Atom{ };
    auto format = 0;
    auto length = 0ul;
    auto rest = 0ul;
    auto data = std::add_pointer_t<unsigned char>{ };
    if (window &&
        XGetWindowProperty(m_display, window, m_net_wm_pid_atom, 0, 1,
          False, XA_CARDINAL, &type, &format, &length,
          &rest, &data) == Success &&
        data) {
      const auto pid = *reinterpret_cast<unsigned long*>(data);
      XFree(data);
      return get_process_path_by_pid(pid);
    }
    return { };
  }
};

std::unique_ptr<FocusedWindowSystem> make_focused_window_x11(FocusedWindowData* data) {
  auto impl = std::make_unique<FocusedWindowX11>(data);
  if (!impl->initialize())
    return { };
  return impl;
}

#endif // ENABLE_X11
