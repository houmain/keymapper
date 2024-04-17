
#include "TrayIcon.h"
#include "common/output.h"

using MakeTrayIconImpl = std::unique_ptr<TrayIcon::IImpl>();
MakeTrayIconImpl make_tray_icon_gtk;

TrayIcon::TrayIcon() = default;
TrayIcon::TrayIcon(TrayIcon&& rhs) noexcept = default;
TrayIcon& TrayIcon::operator=(TrayIcon&& rhs) noexcept = default;
TrayIcon::~TrayIcon() = default;

void TrayIcon::initialize(Handler* handler, bool show_reload) {
#if defined(ENABLE_APPINDICATOR)
  if (auto impl = make_tray_icon_gtk()) 
    if (impl->initialize(handler, show_reload)) {
      m_impl = std::move(impl);
      verbose("Initialized GTK tray icon");
    }
#endif
}

void TrayIcon::update() {
  if (m_impl)
    m_impl->update();
}
