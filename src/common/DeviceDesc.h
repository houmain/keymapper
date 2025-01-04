#pragma once

#include <string>
#include <memory>

struct DeviceDescExt { };

struct DeviceDesc {
  std::string name;
  std::string id;

  std::shared_ptr<const DeviceDescExt> ext;
};
