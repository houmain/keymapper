#pragma once

#include <filesystem>
#include <memory>

class TrayIcon {
public:
  struct Handler {
    virtual void on_toggle_active() = 0;
    virtual void on_open_config() = 0;
    virtual void on_reload_config() = 0;
    virtual void on_request_next_key_info() = 0;
    virtual void on_open_help() = 0;
    virtual void on_open_about() = 0;
    virtual void on_exit() = 0;
  };

  class IImpl {
  public:
    virtual ~IImpl() = default;
    virtual bool initialize(Handler* handler, bool show_reload,
                            const std::filesystem::path& icon_filename,
                            const std::filesystem::path& inactive_icon_filename) = 0;
    virtual void update() = 0;
    virtual void on_active_toggled(bool active) { }
  };

  TrayIcon();
  TrayIcon(TrayIcon&& rhs) noexcept;
  TrayIcon& operator=(TrayIcon&& rhs) noexcept;
  ~TrayIcon();

  void initialize(Handler* handler, bool show_reload,
                  const std::filesystem::path& icon_filename,
                  const std::filesystem::path& inactive_icon_filename);
  void reset();
  void update();
  void on_active_toggled(bool active);

private:
  std::unique_ptr<IImpl> m_impl;
  std::filesystem::path m_icon_filename;
  std::filesystem::path m_inactive_icon_filename;
};
