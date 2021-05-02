
#include "common.h"
#include "config/Key.h"
#include "Wtsapi32.h"
#include <string>

namespace {
  const auto window_class_name = L"Keymapper";
  const auto injected_ident = ULONG_PTR(0xADDED);

  HINSTANCE g_instance;
  HHOOK g_keyboard_hook;
  bool g_sending_key;
  std::vector<INPUT> g_send_buffer;
  bool g_session_changed;

  KeyEvent get_key_event(WPARAM wparam, const KBDLLHOOKSTRUCT& kbd) {
    auto key = static_cast<KeyCode>(kbd.scanCode |
      (kbd.flags & LLKHF_EXTENDED ? 0xE000 : 0));

    // special handling
    if (key == 0xE036)
      key = *Key::ShiftRight;

    auto state = (wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN ?
      KeyState::Down : KeyState::Up);
    return { key, state };
  }

  void send_event(const KeyEvent& event) {
    auto key = INPUT{ };
    key.type = INPUT_KEYBOARD;
    key.ki.dwExtraInfo = injected_ident;
    key.ki.dwFlags |= (event.state == KeyState::Up ? KEYEVENTF_KEYUP : 0);

    // special handling
    if (event.key == *Key::ShiftRight) {
      key.ki.wVk = VK_RSHIFT;
    }
    else {
      key.ki.dwFlags |= KEYEVENTF_SCANCODE;
      if (event.key & 0xE000)
        key.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
      key.ki.wScan = static_cast<WORD>(event.key);
    }
    g_send_buffer.push_back(key);
  }

#if !defined(NDEBUG)
  std::string format(const KeyEvent& e) {
    const auto key_name = [](auto key) {
      auto name = get_key_name(static_cast<Key>(key));
      return (name.empty() ? "???" : std::string(name))
        + " (" + std::to_string(key) + ") ";
    };
    return (e.state == KeyState::Down ? "+" :
            e.state == KeyState::Up ? "-" : "*") + key_name(e.key);
  }

  std::string format(const KeySequence& sequence) {
    auto string = std::string();
    for (const auto& e : sequence)
      string += format(e);
    return string;
  }
#endif // !defined(NDEBUG)

  void flush_send_buffer() {
    g_sending_key = true;
    const auto sent = ::SendInput(g_send_buffer.size(), 
      g_send_buffer.data(), sizeof(INPUT));
    g_send_buffer.clear();
    g_sending_key = false;
  }

  void send_key_sequence(const KeySequence& key_sequence) {
    auto output_on_release = false;
    for (const auto& event : key_sequence) {
      if (event.state == KeyState::OutputOnRelease) {
        flush_send_buffer();
        output_on_release = true;
      }
      else {
        send_event(event);
      }
    }

    if (!output_on_release)
      flush_send_buffer();
  }

  bool translate_keyboard_input(WPARAM wparam, const KBDLLHOOKSTRUCT& kbd) {
    const auto injected = (kbd.dwExtraInfo == injected_ident);
    if (injected || g_sending_key)
      return false;

    const auto input = get_key_event(wparam, kbd);

    // block input after OutputOnRelease until trigger is released
    if (input.state == KeyState::Down && !g_send_buffer.empty())
      return true;

    auto translated = false;
    auto output = apply_input(input);
    if (output.size() != 1 ||
        output.front().key != input.key ||
        (output.front().state == KeyState::Up) != (input.state == KeyState::Up)) {
#if !defined(NDEBUG)
      verbose("%s--> %s", format(input).c_str(), format(output).c_str());
#endif
      send_key_sequence(output);
      translated = true;
    }
    reuse_buffer(std::move(output));

#if !defined(NDEBUG)
    if (!translated)
      verbose("%s", format(input).c_str());
#endif
    return translated;
  }

  LRESULT CALLBACK keyboard_hook_proc(int code, WPARAM wparam, LPARAM lparam) {
    if (code == HC_ACTION) {
      const auto& kbd = *reinterpret_cast<const KBDLLHOOKSTRUCT*>(lparam);
      if (translate_keyboard_input(wparam, kbd))
        return -1;
    }
    return CallNextHookEx(g_keyboard_hook, code, wparam, lparam);
  }

  bool hook_keyboard() {
    if (!g_keyboard_hook)
      g_keyboard_hook = SetWindowsHookExW(
        WH_KEYBOARD_LL, &keyboard_hook_proc, g_instance, 0);
    return (g_keyboard_hook != nullptr);
  }

  void unhook_keyboard() {
    UnhookWindowsHookEx(g_keyboard_hook);
    g_keyboard_hook = nullptr;
  }

  LRESULT CALLBACK window_proc(HWND window, UINT message,
      WPARAM wparam, LPARAM lparam) {
    switch(message) {
      case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

      case WM_WTSSESSION_CHANGE:
        g_session_changed = true;
        break;

      case WM_TIMER: {
        update_configuration();

        // validate state when window was inaccessible
        // force validation after session change
        const auto check_accessibility = 
          !std::exchange(g_session_changed, false);
        validate_state(check_accessibility);

        if (update_focused_window()) {
          // reinsert hook in front of callchain
          unhook_keyboard();
          if (!hook_keyboard())
            verbose("resetting keyboard hook failed");
        }
        break;
      }
    }
    return DefWindowProcW(window, message, wparam, lparam);
  }
} // namespace

int run_hook(HINSTANCE instance) {
  g_instance = instance;

  auto window_class = WNDCLASSEXW{ };
  window_class.cbSize = sizeof(WNDCLASSEXW);
  window_class.lpfnWndProc = &window_proc;
  window_class.hInstance = instance;
  window_class.lpszClassName = window_class_name;
  if (!RegisterClassExW(&window_class))
    return 1;

  verbose("hooking keyboard");
  if (!hook_keyboard()) {
    error("hooking keyboard failed");
    UnregisterClassW(window_class_name, instance);
    return 1;
  }

  auto window = CreateWindowExW(0, window_class_name, NULL, 0,
    CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
    HWND_MESSAGE, NULL, NULL,  NULL);

  WTSRegisterSessionNotification(window, NOTIFY_FOR_THIS_SESSION);
  SetTimer(window, 1, update_interval_ms, NULL);

  verbose("entering update loop");
  auto message = MSG{ };
  while (GetMessageW(&message, window, 0, 0) > 0) {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }

  WTSUnRegisterSessionNotification(window);
  DestroyWindow(window);
  UnregisterClassW(window_class_name, instance);
  unhook_keyboard();
  return 0;
}
