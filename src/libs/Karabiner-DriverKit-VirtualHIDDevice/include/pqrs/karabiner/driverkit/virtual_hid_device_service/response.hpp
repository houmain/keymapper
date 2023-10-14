#pragma once

// (C) Copyright Takayama Fumihiko 2020.
// Distributed under the Boost Software License, Version 1.0.
// (See https://www.boost.org/LICENSE_1_0.txt)

#include <string_view>

namespace pqrs {
namespace karabiner {
namespace driverkit {
namespace virtual_hid_device_service {
enum class response : uint8_t {
  none,
  driver_activated,
  driver_connected,
  driver_version_mismatched,
  virtual_hid_keyboard_ready,
  virtual_hid_pointing_ready,
};
} // namespace virtual_hid_device_service
} // namespace driverkit
} // namespace karabiner
} // namespace pqrs
