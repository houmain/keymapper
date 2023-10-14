#pragma once

// (C) Copyright Takayama Fumihiko 2020.
// Distributed under the Boost Software License, Version 1.0.
// (See https://www.boost.org/LICENSE_1_0.txt)

#include "buttons.hpp"
#include <cstdint>

namespace pqrs {
namespace karabiner {
namespace driverkit {
namespace virtual_hid_device_driver {
namespace hid_report {

class __attribute__((packed)) pointing_input final {
public:
  pointing_input(void) : buttons{}, x(0), y(0), vertical_wheel(0), horizontal_wheel(0) {}
  bool operator==(const pointing_input& other) const { return (memcmp(this, &other, sizeof(*this)) == 0); }
  bool operator!=(const pointing_input& other) const { return !(*this == other); }

  buttons buttons;
  uint8_t x;
  uint8_t y;
  uint8_t vertical_wheel;
  uint8_t horizontal_wheel;
};

} // namespace hid_report
} // namespace virtual_hid_device_driver
} // namespace driverkit
} // namespace karabiner
} // namespace pqrs
