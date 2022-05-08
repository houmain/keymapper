#pragma once

#include <memory>

class GrabbedDevices;
struct FreeGrabbedDevices { void operator()(GrabbedDevices* devices); };
using GrabbedDevicesPtr = std::unique_ptr<GrabbedDevices, FreeGrabbedDevices>;

GrabbedDevicesPtr grab_devices(const char* ignore_device_name, bool grab_mice);
bool read_input_event(GrabbedDevices& devices, int* type, int* code, int* value);
