
#include "HookThread.h"
#include "common/output.h"
#include <thread>
#include <future>
#include <mutex>
#include <utility>

namespace {
  constexpr UINT WM_APP_CONFIGURE_HOOKS = WM_APP + 0;

  KeyboardHookCallback g_keyboard_hook_callback;
  MouseHookCallback g_mouse_hook_callback;
  HHOOK g_keyboard_hook;
  HHOOK g_mouse_hook;
  std::mutex g_hook_thread_mutex;
  std::thread g_hook_thread;
  DWORD g_hook_thread_id;

  struct HookConfig {
    HINSTANCE instance;
    KeyboardHookCallback keyboard_callback;
    MouseHookCallback mouse_callback;
    std::promise<void> completed;
  };

  LRESULT CALLBACK keyboard_hook_proc(int code, WPARAM wparam, LPARAM lparam) {
    if (code == HC_ACTION) {
      const auto& kbd = *reinterpret_cast<const KBDLLHOOKSTRUCT*>(lparam);
      if (g_keyboard_hook_callback && g_keyboard_hook_callback(wparam, kbd))
        return 1;
    }
    return CallNextHookEx(g_keyboard_hook, code, wparam, lparam);
  }

  LRESULT CALLBACK mouse_hook_proc(int code, WPARAM wparam, LPARAM lparam) {
    if (code == HC_ACTION) {
      const auto& ms = *reinterpret_cast<const MSLLHOOKSTRUCT*>(lparam);
      if (g_mouse_hook_callback && g_mouse_hook_callback(wparam, ms))
        return 1;
    }
    return CallNextHookEx(g_mouse_hook, code, wparam, lparam);
  }

  void unhook_devices_on_hook_thread() {
    if (g_keyboard_hook)
      UnhookWindowsHookEx(g_keyboard_hook);
    g_keyboard_hook = nullptr;
    g_keyboard_hook_callback = nullptr;

    if (g_mouse_hook)
      UnhookWindowsHookEx(g_mouse_hook);
    g_mouse_hook = nullptr;
    g_mouse_hook_callback = nullptr;
  }

  void hook_devices_on_hook_thread(const HookConfig& config) {
    const auto keyboard_was_hooked = (g_keyboard_hook_callback != nullptr);
    const auto mouse_was_hooked = (g_mouse_hook_callback != nullptr);

    unhook_devices_on_hook_thread();

    g_keyboard_hook_callback = config.keyboard_callback;
    if (g_keyboard_hook_callback)
      g_keyboard_hook = SetWindowsHookExW(
        WH_KEYBOARD_LL, keyboard_hook_proc, config.instance, 0);

    g_mouse_hook_callback = config.mouse_callback;
    if (g_mouse_hook_callback)
      g_mouse_hook = SetWindowsHookExW(
        WH_MOUSE_LL, mouse_hook_proc, config.instance, 0);

    const auto keyboard_is_hooked = (g_keyboard_hook != nullptr);
    const auto mouse_is_hooked = (g_mouse_hook != nullptr);
    if (keyboard_is_hooked != keyboard_was_hooked)
      verbose(keyboard_is_hooked ? "Hooked keyboard" : "Unhooked keyboard");
    if (mouse_is_hooked != mouse_was_hooked)
      verbose(mouse_is_hooked ? "Hooked mouse" : "Unhooked mouse");
  }

  void hook_thread_main(std::promise<void> ready) {
    g_hook_thread_id = GetCurrentThreadId();

    // create message queue
    auto message = MSG{ };
    PeekMessageW(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
    ready.set_value();

    while (GetMessageW(&message, nullptr, 0, 0) > 0)
      if (message.message == WM_APP_CONFIGURE_HOOKS) {
        auto& config = *reinterpret_cast<HookConfig*>(message.lParam);
        hook_devices_on_hook_thread(config);
        config.completed.set_value();
      }

    unhook_devices_on_hook_thread();
  }

  void start_hook_thread() {
    if (g_hook_thread.joinable())
      return;

    auto ready = std::promise<void>();
    auto ready_future = ready.get_future();
    g_hook_thread = std::thread(hook_thread_main, std::move(ready));
    ready_future.get();
  }

  void configure_devices(HookConfig config) {
    auto completed = config.completed.get_future();
    if (!PostThreadMessageW(g_hook_thread_id, WM_APP_CONFIGURE_HOOKS, 0,
        reinterpret_cast<LPARAM>(&config)))
      return;

    completed.get();
  }
} // namespace

void unhook_devices() {
  const auto lock = std::lock_guard(g_hook_thread_mutex);
  if (g_hook_thread.joinable())
    configure_devices({ });
}

void hook_devices(HINSTANCE instance,
    KeyboardHookCallback keyboard_hook_callback,
    MouseHookCallback mouse_hook_callback) {
  const auto lock = std::lock_guard(g_hook_thread_mutex);
  start_hook_thread();
  configure_devices({ instance, keyboard_hook_callback, mouse_hook_callback });
}

void shutdown_hook_thread() {
  const auto lock = std::lock_guard(g_hook_thread_mutex);
  if (!g_hook_thread.joinable())
    return;

  PostThreadMessageW(g_hook_thread_id, WM_QUIT, 0, 0);
  g_hook_thread.join();
  g_hook_thread_id = 0;
}
