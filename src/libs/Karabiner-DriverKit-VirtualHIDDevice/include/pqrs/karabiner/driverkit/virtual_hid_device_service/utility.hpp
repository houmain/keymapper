#pragma once

// (C) Copyright Takayama Fumihiko 2020.
// Distributed under the Boost Software License, Version 1.0.
// (See https://www.boost.org/LICENSE_1_0.txt)

#include <IOKit/IOKitLib.h>
#include <pqrs/osx/iokit_types.hpp>

namespace pqrs {
namespace karabiner {
namespace driverkit {
namespace virtual_hid_device_service {
namespace utility {
inline bool driver_running(void) {
  auto service = IOServiceGetMatchingService(type_safe::get(pqrs::osx::iokit_mach_port::null),
                                             IOServiceNameMatching("org_pqrs_Karabiner_DriverKit_VirtualHIDDeviceRoot"));
  if (!service) {
    return false;
  }

  IOObjectRelease(service);
  return true;
}

inline bool virtual_hid_keyboard_exists(void) {
  auto service = IOServiceGetMatchingService(type_safe::get(pqrs::osx::iokit_mach_port::null),
                                             IOServiceNameMatching("org_pqrs_Karabiner_DriverKit_VirtualHIDKeyboard"));
  if (!service) {
    return false;
  }

  IOObjectRelease(service);
  return true;
}

inline bool virtual_hid_pointing_exists(void) {
  auto service = IOServiceGetMatchingService(type_safe::get(pqrs::osx::iokit_mach_port::null),
                                             IOServiceNameMatching("org_pqrs_Karabiner_DriverKit_VirtualHIDPointing"));
  if (!service) {
    return false;
  }

  IOObjectRelease(service);
  return true;
}
} // namespace utility
} // namespace virtual_hid_device_service
} // namespace driverkit
} // namespace karabiner
} // namespace pqrs
