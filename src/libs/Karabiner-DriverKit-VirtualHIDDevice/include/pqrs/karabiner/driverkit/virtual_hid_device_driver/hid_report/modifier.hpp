#pragma once

// (C) Copyright Takayama Fumihiko 2020.
// Distributed under the Boost Software License, Version 1.0.
// (See https://www.boost.org/LICENSE_1_0.txt)

#include <cstdint>

namespace pqrs {
namespace karabiner {
namespace driverkit {
namespace virtual_hid_device_driver {
namespace hid_report {

enum class modifier : uint8_t {
  left_control = 0x1 << 0,
  left_shift = 0x1 << 1,
  left_option = 0x1 << 2,
  left_command = 0x1 << 3,
  right_control = 0x1 << 4,
  right_shift = 0x1 << 5,
  right_option = 0x1 << 6,
  right_command = 0x1 << 7,
};
}

} // namespace virtual_hid_device_driver
} // namespace driverkit
} // namespace karabiner
} // namespace pqrs
