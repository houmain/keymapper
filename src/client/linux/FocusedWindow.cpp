
#include "FocusedWindow.h"
#include "common/output.h"
#include <utility>

#if defined(ENABLE_X11)

# include <X11/X.h>
# include <X11/Xlib.h>
# include <X11/Xatom.h>
# include <X11/Xutil.h>
# include <X11/Xos.h>

class FocusedWindowX11 {
public:
  FocusedWindowX11(std::string* focused_window_title,
                   std::string* focused_window_class)
    : m_focused_window_title(*focused_window_title),
      m_focused_window_class(*focused_window_class) {
  }

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
    m_utf8_string_atom = XInternAtom(m_display, "UTF8_STRING", False);
    XSetErrorHandler([](Display*, XErrorEvent*) { return 0; });

    verbose("Initialized X11 focused window detection");
    return true;
  }

  bool update() {
    const auto window = get_focused_window();
    auto window_title = get_window_title(window);
    if (window == m_focused_window &&
        window_title == m_focused_window_title)
      return false;

    // window handles can become invalid any time
    auto window_class = get_window_class(window);
    if (window_class.empty() || window_title.empty())
      return false;

    m_focused_window = window;
    m_focused_window_class = std::move(window_class);
    m_focused_window_title = std::move(window_title);
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

  Display* m_display{ };
  Window m_root_window{ };
  Atom m_net_active_window_atom{ };
  Atom m_net_wm_name_atom{ };
  Atom m_utf8_string_atom{ };
  Window m_focused_window{ };
  std::string& m_focused_window_title;
  std::string& m_focused_window_class;
};

#endif // ENABLE_X11

//-------------------------------------------------------------------------

#if defined(ENABLE_DBUS)

#include <dbus/dbus.h>

const auto dbus_name = "com.github.houmain.Keymapper";
const auto dbus_path = "/com/github/houmain/Keymapper";
const auto dbus_introspection_xml =
  DBUS_INTROSPECT_1_0_XML_DOCTYPE_DECL_NODE
  "<node>\n"
  "  <interface name='org.freedesktop.DBus.Introspectable'>\n"
  "    <method name='Introspect'>\n"
  "      <arg name='data' type='s' direction='out' />\n"
  "    </method>\n"
  "  </interface>\n"
  "  <interface name='com.github.houmain.Keymapper'>\n"
  "    <method name='WindowFocus'>\n"
  "      <arg name='title' direction='in' type='s'/>\n"
  "      <arg name='class' direction='in' type='s'/>\n"
  "    </method>\n"
  "  </interface>\n"
  "</node>\n";

class FocusedWindowDBus {
public:
  FocusedWindowDBus(std::string* focused_window_title,
                    std::string* focused_window_class)
    : m_focused_window_title(*focused_window_title),
      m_focused_window_class(*focused_window_class) {
  }

  ~FocusedWindowDBus() {
    if (m_connection)
      dbus_bus_release_name(m_connection, dbus_name, nullptr);
  }

  bool initialize() {
    auto err = DBusError{ };
    dbus_error_init(&err);

    static const auto server_vtable = DBusObjectPathVTable{
      .message_function = &FocusedWindowDBus::server_message_handler
    };
    m_connection = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (!m_connection ||
        dbus_bus_request_name(m_connection, dbus_name,
          DBUS_NAME_FLAG_REPLACE_EXISTING, &err) != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER ||
        !dbus_connection_register_object_path(m_connection, dbus_path, &server_vtable, this)) {
      error("Initializing D-Bus window detection failed: %s", err.message);
      dbus_error_free(&err);
      return false;
    }
    verbose("Initialized D-BUS focused window detection");
    return true;
  }

  bool update() {
    dbus_connection_read_write_dispatch(m_connection, 0);
    return std::exchange(m_updated, false);
  }

private:
  static DBusHandlerResult server_message_handler(
      DBusConnection* connection, DBusMessage* message, void* user_data) {
    return static_cast<FocusedWindowDBus*>(user_data)->handle_message(message);
  }

  DBusHandlerResult handle_message(DBusMessage* message) {
    auto err = DBusError{ };
    dbus_error_init(&err);

    auto reply = std::add_pointer_t<DBusMessage>();
    if (dbus_message_is_method_call(message,
        DBUS_INTERFACE_INTROSPECTABLE, "Introspect")) {
      if (reply = dbus_message_new_method_return(message); reply)
        dbus_message_append_args(reply, DBUS_TYPE_STRING,
          &dbus_introspection_xml, DBUS_TYPE_INVALID);
    }
    else if (dbus_message_is_method_call(message, dbus_name, "WindowFocus")) {
      auto window_title = std::add_pointer_t<char>();
      auto window_class = std::add_pointer_t<char>();
      if (dbus_message_get_args(message, &err,
          DBUS_TYPE_STRING, &window_title,
          DBUS_TYPE_STRING, &window_class,
          DBUS_TYPE_INVALID)) {
        m_focused_window_title = window_title;
        m_focused_window_class = window_class;
        m_updated = true;
      }
      reply = dbus_message_new_method_return(message);
    }
    else {
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    if (dbus_error_is_set(&err)) {
      dbus_message_unref(reply);
      reply = dbus_message_new_error(message, err.name, err.message);
      dbus_error_free(&err);
    }
    auto result = DBusHandlerResult{ DBUS_HANDLER_RESULT_HANDLED };
    if (!reply || !dbus_connection_send(m_connection, reply, nullptr))
      result = DBUS_HANDLER_RESULT_NEED_MEMORY;
    dbus_message_unref(reply);
    return result;
  }

  DBusConnection* m_connection{ };
  std::string& m_focused_window_title;
  std::string& m_focused_window_class;
  bool m_updated{ };
};

#endif // ENABLE_DBUS

//-------------------------------------------------------------------------

#if defined(ENABLE_WLROOTS)

#include <cstring>
#include <wayland-client.h>
#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"

static const auto WLR_FOREIGN_TOPLEVEL_MANAGEMENT_VERSION = 1;

class FocusedWindowWLRoots {
public:
  FocusedWindowWLRoots(std::string* focused_window_title,
                    std::string* focused_window_class)
    : m_focused_window_title(*focused_window_title),
      m_focused_window_class(*focused_window_class) {
  }

  ~FocusedWindowWLRoots() {
    if (m_display)
      wl_display_disconnect(m_display);
  }

  bool initialize() {
    m_display = wl_display_connect(nullptr);
    if (!m_display)
      return false;

    auto registry = wl_display_get_registry(m_display);
    wl_registry_add_listener(registry, &registry_listener_impl, this);
    wl_display_roundtrip(m_display);

    if (!m_toplevel_manager) {
      error("Initializing Wayland window detection failed: wlr-foreign-toplevel not available");
      return false;
    }
    wl_display_flush(m_display);
    verbose("Initialized Wayland focused window detection");
    return true;
  }

  bool update() {
    wl_display_dispatch(m_display);
    return std::exchange(m_updated, false);
  }

private:
  struct Toplevel {
    FocusedWindowWLRoots* self;
    std::string title;
    std::string app_id;
  };

  static const wl_registry_listener registry_listener_impl;
  static const zwlr_foreign_toplevel_manager_v1_listener toplevel_manager_impl;
  static const zwlr_foreign_toplevel_handle_v1_listener toplevel_impl;

  void handle_global(wl_registry* registry, uint32_t name, const char* interface, uint32_t version) {
    if (std::strcmp(interface, zwlr_foreign_toplevel_manager_v1_interface.name) == 0)
      toplevel_manager_init(static_cast<zwlr_foreign_toplevel_manager_v1*>(
        wl_registry_bind(registry, name, &zwlr_foreign_toplevel_manager_v1_interface,
          WLR_FOREIGN_TOPLEVEL_MANAGEMENT_VERSION)));
  }

  void toplevel_manager_init(zwlr_foreign_toplevel_manager_v1* toplevel_manager) {
    zwlr_foreign_toplevel_manager_v1_add_listener(
      toplevel_manager, &toplevel_manager_impl, this);
    m_toplevel_manager = toplevel_manager;
  }

  void toplevel_manager_finished(zwlr_foreign_toplevel_manager_v1* m_toplevel_manager) {
    zwlr_foreign_toplevel_manager_v1_destroy(m_toplevel_manager);
    m_toplevel_manager = nullptr;
  }

  void toplevel_manager_handle_toplevel(zwlr_foreign_toplevel_handle_v1* handle) {
    auto toplevel = new(std::nothrow) Toplevel();
    if (!toplevel)
      return;
    toplevel->self = this;
    zwlr_foreign_toplevel_handle_v1_add_listener(
      handle, &toplevel_impl, toplevel);
  }

  void toplevel_handle_title(Toplevel& toplevel, const char* title) {
    toplevel.title = title;
  }

  void toplevel_handle_appid(Toplevel& toplevel, const char* app_id) {
    toplevel.app_id = app_id;
  }

  void toplevel_handle_state(Toplevel& toplevel, wl_array* state) {
    const auto begin = static_cast<uint32_t*>(state->data);
    const auto end = begin + state->size / sizeof(uint32_t);
    for (auto it = begin; it != end; ++it)
      if (*it & ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED)
        m_active_toplevel = &toplevel;
  }

  void toplevel_handle_done(Toplevel& toplevel) {
    if (&toplevel == m_active_toplevel) {
      m_focused_window_title = toplevel.title;
      m_focused_window_class = toplevel.app_id;
      m_updated = true;
    }
  }

  void toplevel_handle_closed(Toplevel* toplevel, zwlr_foreign_toplevel_handle_v1 *handle) {
    zwlr_foreign_toplevel_handle_v1_destroy(handle);
    delete toplevel;
  }

  std::string& m_focused_window_title;
  std::string& m_focused_window_class;
  wl_display* m_display{ };
  zwlr_foreign_toplevel_manager_v1* m_toplevel_manager{ };
  void* m_active_toplevel{ };
  bool m_updated{ };
};

const wl_registry_listener FocusedWindowWLRoots::registry_listener_impl {
  // global
  [](void* self, wl_registry* registry,
      uint32_t name, const char* interface, uint32_t version) {
    static_cast<FocusedWindowWLRoots*>(self)->handle_global(registry, name, interface, version);
  },
  // global_remove
  [](void* self, wl_registry* registry, uint32_t name) {
  }
};

const zwlr_foreign_toplevel_manager_v1_listener FocusedWindowWLRoots::toplevel_manager_impl{
  // toplevel
  [](void* self, zwlr_foreign_toplevel_manager_v1* toplevel_manager,
      zwlr_foreign_toplevel_handle_v1* toplevel) {
    static_cast<FocusedWindowWLRoots*>(self)->toplevel_manager_handle_toplevel(toplevel);
  },
  // finished
  [](void* self, zwlr_foreign_toplevel_manager_v1* toplevel_manager) {
    static_cast<FocusedWindowWLRoots*>(self)->toplevel_manager_finished(toplevel_manager);
  }
};

const zwlr_foreign_toplevel_handle_v1_listener FocusedWindowWLRoots::toplevel_impl{
  // title
  [](void* _toplevel, zwlr_foreign_toplevel_handle_v1* handle, const char* title) {
    auto& toplevel = *static_cast<FocusedWindowWLRoots::Toplevel*>(_toplevel);
    toplevel.self->toplevel_handle_title(toplevel, title);
  },
  // app_id
  [](void* _toplevel, zwlr_foreign_toplevel_handle_v1* handle, const char* app_id) {
    auto& toplevel = *static_cast<FocusedWindowWLRoots::Toplevel*>(_toplevel);
    toplevel.self->toplevel_handle_appid(toplevel, app_id);
  },
  // output_enter
  [](void* toplevel, zwlr_foreign_toplevel_handle_v1* handle, wl_output* output) {
  },
  // output_leave
  [](void* toplevel, zwlr_foreign_toplevel_handle_v1* handle, wl_output* output) {
  },
  // state
  [](void* _toplevel, zwlr_foreign_toplevel_handle_v1* handle, wl_array* state) {
    auto& toplevel = *static_cast<FocusedWindowWLRoots::Toplevel*>(_toplevel);
    toplevel.self->toplevel_handle_state(toplevel, state);
  },
  // done
  [](void* _toplevel, zwlr_foreign_toplevel_handle_v1* handle) {
    auto& toplevel = *static_cast<FocusedWindowWLRoots::Toplevel*>(_toplevel);
    toplevel.self->toplevel_handle_done(toplevel);
  },
  // closed
  [](void* _toplevel, zwlr_foreign_toplevel_handle_v1* handle) {
    auto& toplevel = *static_cast<FocusedWindowWLRoots::Toplevel*>(_toplevel);
    toplevel.self->toplevel_handle_closed(&toplevel, handle);
  },
  // parent
  [](void* toplevel, zwlr_foreign_toplevel_handle_v1* handle,
      zwlr_foreign_toplevel_handle_v1* parent) {
  }
};

#endif // ENABLE_WLROOTS

//-------------------------------------------------------------------------

struct FocusedWindow {
  std::string focused_window_title;
  std::string focused_window_class;

#if defined(ENABLE_X11)
  std::unique_ptr<FocusedWindowX11> x11;
#endif

#if defined(ENABLE_DBUS)
  std::unique_ptr<FocusedWindowDBus> dbus;
#endif

#if defined(ENABLE_WLROOTS)
  std::unique_ptr<FocusedWindowWLRoots> wlroots;
#endif
};

void FreeFocusedWindow::operator()(FocusedWindow* window) {
  delete window;
}

FocusedWindowPtr create_focused_window() {
  auto window = FocusedWindowPtr(new FocusedWindow());

#if defined(ENABLE_X11)
  window->x11 = std::make_unique<FocusedWindowX11>(
    &window->focused_window_title, &window->focused_window_class);
  if (!window->x11->initialize())
    window->x11.reset();
#endif

#if defined(ENABLE_DBUS)
  window->dbus = std::make_unique<FocusedWindowDBus>(
    &window->focused_window_title, &window->focused_window_class);
  if (!window->dbus->initialize())
    window->dbus.reset();
#endif

#if defined(ENABLE_WLROOTS)
  window->wlroots = std::make_unique<FocusedWindowWLRoots>(
    &window->focused_window_title, &window->focused_window_class);
  if (!window->wlroots->initialize())
    window->wlroots.reset();
#endif
  return window;
}

bool update_focused_window(FocusedWindow& window) {
#if defined(ENABLE_X11)
  if (window.x11 && window.x11->update())
    return true;
#endif

#if defined(ENABLE_DBUS)
  if (window.dbus && window.dbus->update())
    return true;
#endif

#if defined(ENABLE_WLROOTS)
  if (window.wlroots && window.wlroots->update())
    return true;
#endif
  return false;
}

const std::string& get_class(const FocusedWindow& window) {
  return window.focused_window_class;
}

const std::string& get_title(const FocusedWindow& window) {
  return window.focused_window_title;
}
