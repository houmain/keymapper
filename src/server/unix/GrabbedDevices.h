#pragma once

#include <memory>
#include <optional>
#include <utility>
#include <vector>
#include <string>
#include <chrono>
#include "runtime/KeyEvent.h"

class GrabbedDevices {
public:
  using Duration = std::chrono::duration<double>;

  struct Event {
    int device_index;
    int type;
    int code;
    int value;
  };

  GrabbedDevices();
  GrabbedDevices(GrabbedDevices&&) noexcept;
  GrabbedDevices& operator=(GrabbedDevices&&) noexcept;
  ~GrabbedDevices();

  bool grab(const char* virtual_device_name, bool grab_mice);
  bool update_devices();
  std::pair<bool, std::optional<Event>> read_input_event(
    std::optional<Duration> timeout, int interrupt_fd);
  const std::vector<std::string>& grabbed_device_names() const;

private:
  std::unique_ptr<class GrabbedDevicesImpl> m_impl;
};

std::optional<KeyEvent> to_key_event(const GrabbedDevices::Event& event);
