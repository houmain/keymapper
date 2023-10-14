#pragma once

// (C) Copyright Takayama Fumihiko 2023.
// Distributed under the Boost Software License, Version 1.0.
// (See https://www.boost.org/LICENSE_1_0.txt)

#include "keys.hpp"
#include <cstdint>

namespace pqrs {
namespace karabiner {
namespace driverkit {
namespace virtual_hid_device_driver {
namespace hid_report {

class __attribute__((packed)) generic_desktop_input final {
public:
  generic_desktop_input(void) : report_id_(7) {}
  bool operator==(const generic_desktop_input& other) const { return (memcmp(this, &other, sizeof(*this)) == 0); }
  bool operator!=(const generic_desktop_input& other) const { return !(*this == other); }

private:
  uint8_t report_id_ __attribute__((unused));

public:
  keys keys;
};

} // namespace hid_report
} // namespace virtual_hid_device_driver
} // namespace driverkit
} // namespace karabiner
} // namespace pqrs
