#pragma once

#include <memory>

struct KeyEvent;

class VirtualDevice {
public:
  static constexpr const char* name{ "Keymapper" };
  static constexpr const long vendor_id{ 0xD1CE };
  static constexpr const long product_id{ 0x0001 };

  VirtualDevice();
  VirtualDevice(VirtualDevice&&) noexcept;
  VirtualDevice& operator=(VirtualDevice&&) noexcept;
  ~VirtualDevice();

  bool create();
  bool send_key_event(const KeyEvent& event);
  bool send_event(int type, int code, int value);
  bool flush();

private:
  std::unique_ptr<class VirtualDeviceImpl> m_impl;
};
