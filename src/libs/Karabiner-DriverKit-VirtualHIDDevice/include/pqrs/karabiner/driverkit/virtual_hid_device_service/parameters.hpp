#pragma once

// (C) Copyright Takayama Fumihiko 2024.
// Distributed under the Boost Software License, Version 1.0.
// (See https://www.boost.org/LICENSE_1_0.txt)

#include <pqrs/hid.hpp>
#include <string_view>

namespace pqrs {
namespace karabiner {
namespace driverkit {
namespace virtual_hid_device_service {
class virtual_hid_keyboard_parameters final {
public:
  virtual_hid_keyboard_parameters(void)
      : virtual_hid_keyboard_parameters(pqrs::hid::vendor_id::value_t(0x16c0),
                                        pqrs::hid::product_id::value_t(0x27db),
                                        pqrs::hid::country_code::not_supported) {
  }

  virtual_hid_keyboard_parameters(pqrs::hid::vendor_id::value_t vendor_id,
                                  pqrs::hid::product_id::value_t product_id,
                                  pqrs::hid::country_code::value_t country_code)
      : vendor_id_(vendor_id),
        product_id_(product_id),
        country_code_(country_code) {
  }

  pqrs::hid::vendor_id::value_t get_vendor_id(void) const {
    return vendor_id_;
  }

  void set_vendor_id(pqrs::hid::vendor_id::value_t value) {
    vendor_id_ = value;
  }

  pqrs::hid::product_id::value_t get_product_id(void) const {
    return product_id_;
  }

  void set_product_id(pqrs::hid::product_id::value_t value) {
    product_id_ = value;
  }

  pqrs::hid::country_code::value_t get_country_code(void) const {
    return country_code_;
  }

  void set_country_code(pqrs::hid::country_code::value_t value) {
    country_code_ = value;
  }

  bool operator==(const virtual_hid_keyboard_parameters&) const = default;

private:
  pqrs::hid::vendor_id::value_t vendor_id_;
  pqrs::hid::product_id::value_t product_id_;
  pqrs::hid::country_code::value_t country_code_;
};
} // namespace virtual_hid_device_service
} // namespace driverkit
} // namespace karabiner
} // namespace pqrs
