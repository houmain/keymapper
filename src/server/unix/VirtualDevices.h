#pragma once

#include "common/DeviceDesc.h"
#include <vector>
#include <memory>

struct KeyEvent;

class VirtualDevices {
public:
  static constexpr const char name[]{ "keymapper" };
  static constexpr const long vendor_id{ 0xD1CE };
  static constexpr const long product_id{ 0x0001 };

  VirtualDevices();
  VirtualDevices(VirtualDevices&&) noexcept;
  VirtualDevices& operator=(VirtualDevices&&) noexcept;
  ~VirtualDevices();

  bool create_keyboard_device();
  bool update_forward_devices(const std::vector<DeviceDesc>& device_descs);
  bool send_key_event(const KeyEvent& event);
  bool forward_event(int device_index, int type, int code, int value);
  bool flush();

private:
  std::unique_ptr<class VirtualDevicesImpl> m_impl;
};

#if defined(__APPLE__)
extern bool macos_toggle_fn;
#endif
