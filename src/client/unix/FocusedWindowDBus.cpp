
#if defined(ENABLE_DBUS)

#include "FocusedWindowImpl.h"
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

class FocusedWindowDBus : public FocusedWindowSystem {
private:
  FocusedWindowData& m_data;
  DBusConnection* m_connection{ };
  bool m_registered_object_path{ };
  bool m_updated{ };

  struct DBusErrorObject : DBusError {
    DBusErrorObject() { dbus_error_init(this); }
    ~DBusErrorObject() { dbus_error_free(this); }
    DBusErrorObject(DBusErrorObject&&) = default;
    DBusErrorObject& operator=(DBusErrorObject&&) = default;
  };

public:
  explicit FocusedWindowDBus(FocusedWindowData* data)
    : m_data(*data) {
  }

  FocusedWindowDBus(const FocusedWindowDBus&) = delete;
  FocusedWindowDBus& operator=(const FocusedWindowDBus&) = delete;

  ~FocusedWindowDBus() {
    if (m_registered_object_path)
      dbus_connection_unregister_object_path(m_connection, dbus_path);
    if (m_connection)
      dbus_bus_release_name(m_connection, dbus_name, nullptr);
  }

  bool initialize() {
    auto err = DBusErrorObject{ };
    m_connection = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (!m_connection)
      return false;

    if (dbus_bus_request_name(m_connection, dbus_name, 0, &err) !=
          DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
      return false;

    static const auto server_vtable = DBusObjectPathVTable{
      nullptr,
      &FocusedWindowDBus::server_message_handler
    };
    if (!dbus_connection_register_object_path(m_connection, dbus_path, &server_vtable, this))
      return false;
    m_registered_object_path = true;
    return true;
  }

  bool update() override {
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
      reply = dbus_message_new_method_return(message);
      if (reply)
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
        m_data.window_title = window_title;
        m_data.window_class = window_class;
        m_data.window_path = "";
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
};

std::unique_ptr<FocusedWindowSystem> make_focused_window_dbus(FocusedWindowData* data) {
  auto impl = std::make_unique<FocusedWindowDBus>(data);
  if (!impl->initialize())
    return { };
  return impl;
}

#endif // ENABLE_DBUS
