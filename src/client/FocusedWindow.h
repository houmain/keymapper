#pragma once

#include <memory>
#include <string>

class FocusedWindow {
public:
  FocusedWindow();
  FocusedWindow(FocusedWindow&& rhs) noexcept;
  FocusedWindow& operator=(FocusedWindow&& rhs) noexcept;
  ~FocusedWindow();

  bool initialize();
  void shutdown();
  bool update();
  const std::string& window_class() const;
  const std::string& window_title() const;
  const std::string& window_path() const;
  bool is_inaccessible() const;

private:
  std::unique_ptr<class FocusedWindowImpl> m_impl;
};
