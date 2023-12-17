#pragma once

// (C) Copyright Takayama Fumihiko 2020.
// Distributed under the Boost Software License, Version 1.0.
// (See https://www.boost.org/LICENSE_1_0.txt)

#include <string_view>

namespace pqrs {
namespace karabiner {
namespace driverkit {
namespace virtual_hid_device_service {
enum class request : uint8_t {
  none,
  virtual_hid_keyboard_initialize,
  virtual_hid_keyboard_terminate,
  virtual_hid_keyboard_reset,
  virtual_hid_pointing_initialize,
  virtual_hid_pointing_terminate,
  virtual_hid_pointing_reset,
  post_keyboard_input_report,
  post_consumer_input_report,
  post_apple_vendor_keyboard_input_report,
  post_apple_vendor_top_case_input_report,
  post_generic_desktop_input_report,
  post_pointing_input_report,
};
} // namespace virtual_hid_device_service
} // namespace driverkit
} // namespace karabiner
} // namespace pqrs
