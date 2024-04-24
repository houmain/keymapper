
#if defined(ENABLE_APPINDICATOR)

#include "TrayIcon.h"
#include <gtk/gtk.h>
#include <libappindicator/app-indicator.h>

namespace {
  template<typename T>
  struct implicitly_castable_t {
    T value;

    template<typename U>
    constexpr operator U() const { return reinterpret_cast<U>(value); }
  };
  
  template<typename T>
  constexpr auto cast(T&& value) {
    return implicitly_castable_t<T>{ value };
  }  
} // namespace

class TrayIconGtk : public TrayIcon::IImpl {
private:
  using Handler = TrayIcon::Handler;

  template<auto Method>
  static void callback(GtkWidget* widget, gpointer data) {
    (static_cast<Handler*>(data)->*Method)();
  }
    
  AppIndicator* m_app_indicator{ };

public:
  ~TrayIconGtk() {
    if (m_app_indicator)
      g_object_unref(m_app_indicator);
  }
  
  bool initialize(Handler* handler, bool show_reload) override {
    if (!gtk_init_check(0, nullptr))
      return false;
    
    auto menu = cast(gtk_menu_new());
    const auto append = [&](auto item, auto callback) {
      gtk_menu_shell_append(menu, item);
      g_signal_connect(item, "activate", cast(callback), handler);
    };
    auto active = gtk_check_menu_item_new_with_label("Active");
    gtk_check_menu_item_set_active(cast(active), true);
    append(active, callback<&Handler::on_toggle_active>);
    append(gtk_menu_item_new_with_label("Configuration"),
      callback<&Handler::on_open_config>);
    if (show_reload)
      append(gtk_menu_item_new_with_label("Reload"),
        callback<&Handler::on_reload_config>);
    append(gtk_menu_item_new_with_label("Devices"),
      callback<&Handler::on_open_devices>);
    append(gtk_menu_item_new_with_label("Help"),
      callback<&Handler::on_open_help>);
    append(gtk_menu_item_new_with_label("About"),
      callback<&Handler::on_open_about>);
    append(gtk_separator_menu_item_new(),
      callback<&Handler::on_open_about>);
    append(gtk_menu_item_new_with_label("Exit"),
      callback<&Handler::on_exit>);
    
    m_app_indicator = app_indicator_new("keymapper", "io.github.houmain.keymapper",
      APP_INDICATOR_CATEGORY_APPLICATION_STATUS);
    app_indicator_set_status(m_app_indicator, APP_INDICATOR_STATUS_ACTIVE);
    app_indicator_set_menu(m_app_indicator, menu);
    gtk_widget_show_all(menu);
    return true;
  }

  void update() override {
    while (gtk_events_pending())
      gtk_main_iteration();
  }
};

std::unique_ptr<TrayIcon::IImpl> make_tray_icon_gtk() {
  return std::make_unique<TrayIconGtk>();
}

#endif // ENABLE_APPINDICATOR
