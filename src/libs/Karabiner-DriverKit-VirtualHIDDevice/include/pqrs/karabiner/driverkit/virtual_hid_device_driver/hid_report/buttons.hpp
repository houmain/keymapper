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

class __attribute__((packed)) buttons final {
public:
  buttons(void) : buttons_(0) {}

  uint32_t get_raw_value(void) const {
    return buttons_;
  }

  bool empty(void) const {
    return buttons_ == 0;
  }

  void clear(void) {
    buttons_ = 0;
  }

  void insert(uint8_t button) {
    if (1 <= button && button <= 32) {
      buttons_ |= (0x1 << (button - 1));
    }
  }

  void erase(uint8_t button) {
    if (1 <= button && button <= 32) {
      buttons_ &= ~(0x1 << (button - 1));
    }
  }

  bool exists(uint8_t button) const {
    if (1 <= button && button <= 32) {
      return buttons_ & (0x1 << (button - 1));
    }

    return false;
  }

  bool operator==(const buttons& other) const { return (memcmp(this, &other, sizeof(*this)) == 0); }
  bool operator!=(const buttons& other) const { return !(*this == other); }

private:
  uint32_t buttons_; // 32 bits for each button (32 buttons)

  // (0x1 << 0) -> button 1
  // (0x1 << 1) -> button 2
  // (0x1 << 2) -> button 3
  // ...
  // (0x1 << 0) -> button 9
  // ...
  // (0x1 << 0) -> button 17
  // ...
  // (0x1 << 0) -> button 25
};

} // namespace hid_report
} // namespace virtual_hid_device_driver
} // namespace driverkit
} // namespace karabiner
} // namespace pqrs
