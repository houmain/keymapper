#pragma once

#include <memory>

class GrabbedKeyboards;
struct FreeGrabbedKeyboards { void operator()(GrabbedKeyboards* keyboards); };
using GrabbedKeyboardsPtr = std::unique_ptr<GrabbedKeyboards, FreeGrabbedKeyboards>;

GrabbedKeyboardsPtr grab_keyboards(const char* ignore_device_name);
bool read_keyboard_event(GrabbedKeyboards& keyboards, int* type, int* code, int* value);
