#pragma once

#include <memory>

class KeyEvent;

class UinputDevice {
public:
  UinputDevice();
  UinputDevice(UinputDevice&&) noexcept;
  UinputDevice& operator=(UinputDevice&&) noexcept;
  ~UinputDevice();

  bool create(const char* name);
  bool send_key_event(const KeyEvent& event);
  bool send_event(int type, int code, int value);

private:
  std::unique_ptr<class UinputDeviceImpl> m_impl;
};
