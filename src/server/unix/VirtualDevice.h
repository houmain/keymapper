#pragma once

#include <memory>

struct KeyEvent;

class VirtualDevice {
public:
  VirtualDevice();
  VirtualDevice(VirtualDevice&&) noexcept;
  VirtualDevice& operator=(VirtualDevice&&) noexcept;
  ~VirtualDevice();

  bool create(const char* name);
  bool send_key_event(const KeyEvent& event);
  bool send_event(int type, int code, int value);
  bool flush();

private:
  std::unique_ptr<class VirtualDeviceImpl> m_impl;
};
