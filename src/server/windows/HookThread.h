
#include "common/windows/win.h"

using KeyboardHookCallback = bool(*)(WPARAM, const KBDLLHOOKSTRUCT& kbd);
using MouseHookCallback = bool(*)(WPARAM, const MSLLHOOKSTRUCT& ms);

void hook_devices(HINSTANCE instance,
  KeyboardHookCallback keyboard_hook_callback,
  MouseHookCallback mouse_hook_callback);
void unhook_devices();
void shutdown_hook_thread();
