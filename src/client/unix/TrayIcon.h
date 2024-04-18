#pragma once

#include <memory>

class TrayIcon {
public:
  struct Handler {
    virtual void on_toggle_active() = 0;
    virtual void on_open_config() = 0;
    virtual void on_reload_config() = 0;
    virtual void on_open_devices() = 0;
    virtual void on_open_help() = 0;
    virtual void on_open_about() = 0;
    virtual void on_exit() = 0;
  };

  class IImpl {
  public:
    virtual ~IImpl() = default;
    virtual bool initialize(Handler* handler, bool show_reload) = 0;
    virtual void update() = 0;
  };

  TrayIcon();
  TrayIcon(TrayIcon&& rhs) noexcept;
  TrayIcon& operator=(TrayIcon&& rhs) noexcept;
  ~TrayIcon();

  void initialize(Handler* handler, bool show_reload);
  void update();

private:
  std::unique_ptr<IImpl> m_impl;
};
