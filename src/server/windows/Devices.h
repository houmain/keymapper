#pragma once

#include "runtime/KeyEvent.h"
#include "common/windows/win.h"
#include "common/DeviceDesc.h"
#include "common/Filter.h"
#include <vector>
#include <memory>

class Devices {
public:
  Devices();
  Devices(const Devices&) = delete;
  Devices& operator=(const Devices&) = delete;
  ~Devices();

  bool initialize(HWND window, UINT input_message);
  bool initialized();
  void shutdown();
  void on_device_attached(HANDLE device);
  void on_device_removed(HANDLE device);
  int get_device_index(HANDLE device) const;
  void set_grab_filters(std::vector<GrabDeviceFilter> filters);
  const std::string& error_message() const { return m_error_message; }
  const std::vector<DeviceDesc>& device_descs() const { return m_device_descs; }
  void send_input(const KeyEvent& event);

private:
  void reset_device_filters();
  void apply_device_filters();

  HWND m_window{ };
  std::vector<GrabDeviceFilter> m_grab_filters;
  std::vector<HANDLE> m_device_handles;
  std::vector<DeviceDesc> m_device_descs;
  std::vector<HANDLE> m_ignored_device_handles;
  std::vector<DeviceDesc> m_ignored_device_descs;
  std::unique_ptr<class Interception> m_interception;
  std::string m_error_message;
};
