#pragma once

#include "common/DeviceDesc.h"
#include <vector>
#include <cstdint>

struct DeviceDescLinux : DeviceDescExt {

  struct AbsAxis {
    int code;
    int value;
    int minimum;
    int maximum;
    int fuzz;
    int flat;
    int resolution;
  };

  int event_id;
  long vendor_id;
  long product_id;
  long version_id;
  std::vector<uint16_t> keys;
  std::vector<AbsAxis> abs_axes;
  uint64_t rel_axes;
  uint64_t rep_events;
  uint64_t switch_events;
  uint64_t misc_events;
  uint64_t properties;
};
