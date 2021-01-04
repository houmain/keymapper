
#include "FocusedWindow.h"

#if defined(ENABLE_X11)

# include <X11/X.h>
# include <X11/Xlib.h>
# include <X11/Xatom.h>
# include <X11/Xutil.h>
# include <X11/Xos.h>

class FocusedWindow {
public:
  ~FocusedWindow() {
    if (m_display)
      XCloseDisplay(m_display);
  }

  const std::string& get_class() const { return m_focused_window_class; }
  const std::string& get_title() const { return m_focused_window_title; }

  bool initialize() {
    m_display = XOpenDisplay(nullptr);
    if (!m_display)
      return false;

    m_root_window = XRootWindow(m_display, 0);
    m_net_active_window_atom = XInternAtom(m_display, "_NET_ACTIVE_WINDOW", False);
    m_net_wm_name_atom = XInternAtom(m_display, "_NET_WM_NAME", False);
    m_utf8_string_atom = XInternAtom(m_display, "UTF8_STRING", False);
    return true;
  }

  bool update() {
    const auto window = get_focused_window();
    if (window == m_focused_window)
      return false;

    // TODO: is there a better way to prevent races?
    const auto handler = XSetErrorHandler(&ignore_errors);
    m_focused_window = window;
    m_focused_window_class = get_window_class(window);
    m_focused_window_title = get_window_title(window);
    XSetErrorHandler(handler);

    return true;
  }

private:
  static int ignore_errors(Display*, XErrorEvent*) { return 0; }

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
    if (m_focused_window &&
        XGetClassHint(m_display, window, &ch) != 0) {
      const auto result = std::string(ch.res_name);
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
    if (m_focused_window &&
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

  Display* m_display{ };
  Window m_root_window{ };
  Atom m_net_active_window_atom{ };
  Atom m_net_wm_name_atom{ };
  Atom m_utf8_string_atom{ };
  Window m_focused_window{ };
  std::string m_focused_window_class;
  std::string m_focused_window_title;
};

void FreeFocusedWindow::operator()(FocusedWindow* window) {
  delete window;
}

FocusedWindowPtr create_focused_window() {
  auto window = FocusedWindowPtr(new FocusedWindow());
  if (!window->initialize())
    return nullptr;
  return window;
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

#else // !ENABLE_X11

void FreeFocusedWindow::operator()(FocusedWindow*) {
}

FocusedWindowPtr create_focused_window() {
  return nullptr;
}

bool update_focused_window(FocusedWindow&) {
  return false;
}

const std::string& get_class(const FocusedWindow&) {
  static std::string empty;
  return empty;
}

const std::string& get_title(const FocusedWindow& window) {
  return get_class(window);
}

#endif // !ENABLE_X11
