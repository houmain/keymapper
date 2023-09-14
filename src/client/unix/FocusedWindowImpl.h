#pragma once

#include "client/FocusedWindow.h"
#include <vector>
#include <utility>

struct FocusedWindowData {
  std::string window_class;
  std::string window_title;
  std::string window_path;
};

class FocusedWindowSystem {
public:
  virtual ~FocusedWindowSystem() = default;
  virtual bool update() = 0;
};

class FocusedWindowImpl : public FocusedWindowData {
private:
  std::vector<std::unique_ptr<FocusedWindowSystem>> m_systems;

public:
  bool initialize();
  void shutdown();
  bool update();
};

std::string get_process_path_by_pid(int pid);
