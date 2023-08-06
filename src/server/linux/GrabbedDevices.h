#pragma once

#include <memory>
#include <optional>
#include <utility>
#include <vector>
#include <string>
#include "common/Duration.h"

class GrabbedDevices {
public:
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

  bool grab(const char* ignore_device_name, bool grab_mice);
  std::pair<bool, std::optional<Event>> read_input_event(
    std::optional<Duration> timeout, int interrupt_fd);
  const std::vector<std::string>& grabbed_device_names() const;

private:
  std::unique_ptr<class GrabbedDevicesImpl> m_impl;
};
