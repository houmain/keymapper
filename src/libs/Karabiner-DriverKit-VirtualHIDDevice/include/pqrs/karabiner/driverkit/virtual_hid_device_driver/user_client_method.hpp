#pragma once

// (C) Copyright Takayama Fumihiko 2020.
// Distributed under the Boost Software License, Version 1.0.
// (See https://www.boost.org/LICENSE_1_0.txt)

namespace pqrs {
namespace karabiner {
namespace driverkit {
namespace virtual_hid_device_driver {
enum class user_client_method {
  driver_version,

  //
  // keyboard
  //

  virtual_hid_keyboard_initialize,
  virtual_hid_keyboard_ready,
  virtual_hid_keyboard_post_report,
  virtual_hid_keyboard_reset,

  //
  // pointing
  //

  virtual_hid_pointing_initialize,
  virtual_hid_pointing_ready,
  virtual_hid_pointing_post_report,
  virtual_hid_pointing_reset,
};
} // namespace virtual_hid_device_driver
} // namespace driverkit
} // namespace karabiner
} // namespace pqrs
