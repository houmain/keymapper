#pragma once

#include "runtime/Key.h"
#include "DeviceDesc.h"

struct KeyInfo {
  Key key;
  DeviceDesc device;
  std::string window_class;
  std::string window_title;
  std::string window_path;
};
