#pragma once

// (C) Copyright Takayama Fumihiko 2020.
// Distributed under the Boost Software License, Version 1.0.
// (See https://www.boost.org/LICENSE_1_0.txt)

#include "modifier.hpp"
#include <cstdint>

namespace pqrs {
namespace karabiner {
namespace driverkit {
namespace virtual_hid_device_driver {
namespace hid_report {

class __attribute__((packed)) modifiers final {
public:
  modifiers(void) : modifiers_(0) {}

  uint8_t get_raw_value(void) const {
    return modifiers_;
  }

  bool empty(void) const {
    return modifiers_ == 0;
  }

  void clear(void) {
    modifiers_ = 0;
  }

  void insert(modifier value) {
    modifiers_ |= static_cast<uint8_t>(value);
  }

  void erase(modifier value) {
    modifiers_ &= ~(static_cast<uint8_t>(value));
  }

  bool exists(modifier value) const {
    return modifiers_ & static_cast<uint8_t>(value);
  }

  bool operator==(const modifiers& other) const { return (memcmp(this, &other, sizeof(*this)) == 0); }
  bool operator!=(const modifiers& other) const { return !(*this == other); }

private:
  uint8_t modifiers_;
};

} // namespace hid_report
} // namespace virtual_hid_device_driver
} // namespace driverkit
} // namespace karabiner
} // namespace pqrs
