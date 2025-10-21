
#include "server/Settings.h"
#include "server/ServerState.h"
#include "runtime/Timeout.h"
#include "common/windows/LimitSingleInstance.h"
#include "common/output.h"
#include "Devices.h"
#include <WinSock2.h>

namespace {
  class ServerStateImpl final : public ServerState {
    bool on_send_key(const KeyEvent& event) override;
    bool on_flushed_send_buffer() override;
    void on_flush_scheduled(Duration delay) override;
    void on_timeout_scheduled(Duration timeout) override;
    void on_timeout_cancelled() override;
    void on_exit_requested() override;
    void on_grab_device_filters_message(
        std::vector<GrabDeviceFilter> filters) override;
    bool on_validate_key_is_down(Key key) override;
    std::string get_devices_error_message() override;
  };
  
  // Calling SendInput directly from mouse hook proc seems to trigger a
  // timeout, therefore it is called after returning from the hook proc. 
  // But for keyboard input it is still more reliable to call it directly!
  const auto TIMER_FLUSH_SEND_BUFFER = 1;
  const auto TIMER_TIMEOUT = 2;
  const auto WM_APP_CLIENT_MESSAGE = WM_APP + 0;
  const auto WM_APP_DEVICE_INPUT = WM_APP + 1;
  const auto injected_ident = ULONG_PTR(0xADDED);
  const auto keyboard_device_index = 0;
  const auto mouse_device_index = 1;

  HINSTANCE g_instance;
  HWND g_window;
  HHOOK g_keyboard_hook;
  HHOOK g_mouse_hook;
  std::vector<Key> g_buttons_down;
  Devices g_devices;
  ServerStateImpl g_state;
  std::vector<INPUT> g_input_buffer;

  KeyEvent get_key_event(WPARAM wparam, const KBDLLHOOKSTRUCT& kbd) {
    // ignore unknown events
    if (kbd.scanCode > 0xFF)
      return { };

    auto key_code = (kbd.scanCode ? 
      kbd.scanCode : MapVirtualKeyW(kbd.vkCode, MAPVK_VK_TO_VSC));

    if (!key_code)
      return { };

    if (kbd.flags & LLKHF_EXTENDED)
      key_code |= 0xE000;

    // special handling
    if (key_code == 0xE036)
      key_code = *Key::ShiftRight;

    auto state = (wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN ?
      KeyState::Down : KeyState::Up);
    return { static_cast<Key>(key_code), state };
  }

  std::optional<INPUT> make_key_input(const KeyEvent& event) {
    auto key = INPUT{ };
    key.type = INPUT_KEYBOARD;
    key.ki.dwExtraInfo = injected_ident;
    key.ki.dwFlags |= (event.state == KeyState::Up ? KEYEVENTF_KEYUP : 0);
    key.ki.time = GetTickCount();

    switch (event.key) {
      case Key::unicode_output: {
        // since KeyEvent::value is too small to hold a 16bit
        // character code it is sent in two consecutive events
        static WORD s_high_byte_down;
        static WORD s_high_byte_up;
        auto& high_byte = (event.state == KeyState::Up ? s_high_byte_up : s_high_byte_down);
        if (high_byte) {
          key.ki.dwFlags |= KEYEVENTF_UNICODE;
          key.ki.wScan = ((high_byte << 8) | event.value);
          high_byte = { };
        }
        else {
          // set unused high bit to discriminate zero from not-set
          high_byte = (0x8000 | event.value);
          return std::nullopt;
        }
        break;
      }
      case Key::ShiftRight: key.ki.wVk = VK_RSHIFT; break;
      case Key::Pause:      key.ki.wVk = VK_PAUSE; break;
      case Key::NumLock:    key.ki.wVk = VK_NUMLOCK; break;
      case Key::MetaLeft:   key.ki.wVk = VK_LWIN; break;
      case Key::MetaRight:  key.ki.wVk = VK_RWIN; break;
      default:
        key.ki.dwFlags |= KEYEVENTF_SCANCODE;
        if (*event.key & 0xE000)
          key.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
        key.ki.wScan = static_cast<WORD>(event.key);
    }
    return key;
  }

  bool prevent_button_repeat(const KeyEvent& event) {
    if (!is_mouse_button(event.key))
      return false;

    const auto it = std::find(g_buttons_down.begin(), g_buttons_down.end(), event.key);
    if (event.state == KeyState::Down) {
      if (it != g_buttons_down.end())
        return true;
      g_buttons_down.push_back(event.key);
    }
    else {
      if (it == g_buttons_down.end())
        return true;
      g_buttons_down.erase(it);
    }
    return false;
  }

  std::optional<INPUT> make_button_input(const KeyEvent& event) {
    const auto down = (event.state == KeyState::Down);
    auto button = INPUT{ };
    button.mi.dwExtraInfo = injected_ident;
    button.mi.time = GetTickCount();
    button.type = INPUT_MOUSE;
    switch (event.key) {
      case Key::ButtonLeft:
        button.mi.dwFlags = (down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP);
        break;
      case Key::ButtonRight:
        button.mi.dwFlags = (down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP);
        break;
      case Key::ButtonMiddle:
        button.mi.dwFlags = (down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP);
        break;
      case Key::ButtonBack:
        button.mi.dwFlags = (down ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP);
        button.mi.mouseData = XBUTTON1;
        break;
      case Key::ButtonForward:
        button.mi.dwFlags = (down ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP);
        button.mi.mouseData = XBUTTON2;
        break;
      case Key::WheelDown:
      case Key::WheelUp:
      case Key::WheelLeft:
      case Key::WheelRight: {
        const auto vertical = (event.key == Key::WheelUp || event.key == Key::WheelDown);
        const auto negative = (event.key == Key::WheelDown || event.key == Key::WheelLeft);
        const auto value = (event.value ? event.value : WHEEL_DELTA) * (negative ? -1 : 1);
        button.mi.dwFlags = (vertical ? MOUSEEVENTF_WHEEL : MOUSEEVENTF_HWHEEL);
        button.mi.mouseData = static_cast<DWORD>(value);
        break;
      }
      default:
        return { };
    }
    return button;
  }

  UINT to_milliseconds(Duration duration) {
    return static_cast<UINT>(std::chrono::duration_cast<
          std::chrono::milliseconds>(duration).count());
  }

  bool ServerStateImpl::on_send_key(const KeyEvent& event) {
    if (prevent_button_repeat(event))
      return true;

    if (g_devices.initialized() && g_devices.send_input(event))
      return true;

    auto input = make_button_input(event);
    if (!input.has_value())
      input = make_key_input(event);
    if (input.has_value())
      g_input_buffer.emplace_back(input.value());
    return true;
  }

  // unique, which allows modification of the predicate's first operand
  template <class ForwardIt, class BinaryPredicate>
  ForwardIt unique_mutable(ForwardIt first, ForwardIt last, BinaryPredicate p) {
    if (first == last)
      return last;
    auto result = first;
    while (++first != last)
      if (!p(*result, *first) && ++result != first)
        *result = std::move(*first);
    return ++result;
  }

  bool merge_wheel_event(INPUT& a, const INPUT& b) {
    if (a.type == INPUT_MOUSE &&
        b.type == INPUT_MOUSE &&
        a.mi.dwFlags == b.mi.dwFlags) {
      const auto value_a = static_cast<int>(a.mi.mouseData);
      const auto value_b = static_cast<int>(b.mi.mouseData);
      if (std::signbit(value_a) == std::signbit(value_b)) {
        a.mi.mouseData = static_cast<DWORD>(value_a + value_b);
        return true;
      }
    }
    return false;
  }

  void merge_wheel_events(std::vector<INPUT>& input) {
    input.erase(unique_mutable(begin(input), end(input), 
      &merge_wheel_event), end(input));
  }

  bool ServerStateImpl::on_flushed_send_buffer() {
    // merge wheel events, since sending many can lag severly
    merge_wheel_events(g_input_buffer);

    // sending each input separately, since Windows starts beeping
    // when sending multiple mouse click events at once
    for (auto& input : g_input_buffer)
      ::SendInput(1, &input, sizeof(input));

    g_input_buffer.clear();
    return true;
  }

  void ServerStateImpl::on_flush_scheduled(Duration delay) {
    ::SetTimer(g_window, TIMER_FLUSH_SEND_BUFFER,
      to_milliseconds(delay), nullptr);
  }

  void ServerStateImpl::on_timeout_scheduled(Duration timeout) {
    ::SetTimer(g_window, TIMER_TIMEOUT, 
      to_milliseconds(timeout),  nullptr);
  }

  void ServerStateImpl::on_timeout_cancelled() {
    ::KillTimer(g_window, TIMER_TIMEOUT);
  }

  void ServerStateImpl::on_exit_requested() {
    ::DestroyWindow(g_window);
  }

  void ServerStateImpl::on_grab_device_filters_message(
        std::vector<GrabDeviceFilter> filters) {
    g_devices.set_grab_filters(std::move(filters));
    g_state.set_device_descs(g_devices.device_descs());
  }

  std::string ServerStateImpl::get_devices_error_message() {
    return g_devices.error_message();
  }

  bool translate_keyboard_input(WPARAM wparam, const KBDLLHOOKSTRUCT& kbd) {
    const auto injected = (kbd.dwExtraInfo == injected_ident);
    if (injected)
      return false;

    // ignore remote desktop input
    if (kbd.dwExtraInfo == 0x4321DCBA)
      return false;

    const auto input = get_key_event(wparam, kbd);
    if (input.key == Key::none) {
      verbose("sc: 0x%X, vk: 0x%X", kbd.scanCode, kbd.vkCode);

      // ControlRight preceding AltGr, intercept when it was not sent
      const auto ControlRightPrecedingAltGr = 0x21D;
      if (kbd.scanCode == ControlRightPrecedingAltGr)
        return !g_state.sending_key();

      return false;
    }

    return g_state.translate_input(input, keyboard_device_index);
  }

  LRESULT CALLBACK keyboard_hook_proc(int code, WPARAM wparam, LPARAM lparam) {
    if (code == HC_ACTION) {
      const auto& kbd = *reinterpret_cast<const KBDLLHOOKSTRUCT*>(lparam);
      if (translate_keyboard_input(wparam, kbd)) {
        // never send mouse events directly from the keyboard hook proc since it can block
        if (g_state.send_buffer_has_mouse_events()) {
          g_state.schedule_flush();
        }
        else {
          if (!g_state.flush_scheduled_at())
            g_state.flush_send_buffer();
        }
        return -1;
      }
    }
    return CallNextHookEx(g_keyboard_hook, code, wparam, lparam);
  }

  std::optional<KeyEvent> get_button_event(WPARAM wparam, const MSLLHOOKSTRUCT& ms) {
    auto state = KeyState::Down;
    auto key = Key::none;
    auto value = KeyEvent::value_t{ };
    switch (wparam) {
      case WM_LBUTTONDOWN: key = Key::ButtonLeft; break;
      case WM_RBUTTONDOWN: key = Key::ButtonRight; break;
      case WM_MBUTTONDOWN: key = Key::ButtonMiddle; break;
      case WM_LBUTTONUP:   key = Key::ButtonLeft; state = KeyState::Up; break;
      case WM_RBUTTONUP:   key = Key::ButtonRight; state = KeyState::Up; break;
      case WM_MBUTTONUP:   key = Key::ButtonMiddle; state = KeyState::Up; break;
      case WM_XBUTTONDOWN: 
      case WM_XBUTTONUP:
        key = ((HIWORD(ms.mouseData) & XBUTTON1) ? Key::ButtonBack : Key::ButtonForward);
        state = (wparam == WM_XBUTTONDOWN ? KeyState::Down : KeyState::Up);
        break;
      case WM_MOUSEWHEEL:
      case WM_MOUSEHWHEEL: {
        const auto delta = GET_WHEEL_DELTA_WPARAM(ms.mouseData);
        key = (wparam == WM_MOUSEWHEEL ?
          (delta < 0 ? Key::WheelDown : Key::WheelUp) :
          (delta < 0 ? Key::WheelLeft : Key::WheelRight));
        state = KeyState::Up; // Down is inserted by server
        value = static_cast<KeyEvent::value_t>(std::abs(delta));
        break;
      }
      default:
        return { };
    }
    return KeyEvent{ key, state, value };
  }

  bool translate_mouse_input(WPARAM wparam, const MSLLHOOKSTRUCT& ms) {
    const auto injected = (ms.dwExtraInfo == injected_ident);
    if (injected)
      return false;
    
    const auto input = get_button_event(wparam, ms);
    if (!input.has_value())
      return false;

    if (g_state.translate_input(*input, mouse_device_index))
      return true;

    return prevent_button_repeat(*input);
  }

  LRESULT CALLBACK mouse_hook_proc(int code, WPARAM wparam, LPARAM lparam) {
    if (code == HC_ACTION) {
      const auto& ms = *reinterpret_cast<const MSLLHOOKSTRUCT*>(lparam);
      if (translate_mouse_input(wparam, ms)) {
        g_state.schedule_flush();
        return -1;
      }
    }
    return CallNextHookEx(g_mouse_hook, code, wparam, lparam);
  }

  void unhook_devices() {
    if (auto hook = std::exchange(g_keyboard_hook, nullptr))
      UnhookWindowsHookEx(hook);
    if (auto hook = std::exchange(g_mouse_hook, nullptr))
      UnhookWindowsHookEx(hook);
  }

  void hook_devices() {
    unhook_devices();
    
    if (g_devices.initialized())
      return;

    g_keyboard_hook = SetWindowsHookExW(
      WH_KEYBOARD_LL, &keyboard_hook_proc, g_instance, 0);
    if (!g_keyboard_hook)
      error("Hooking keyboard failed");

#if !defined(NDEBUG)
    // do not hook mouse while debugging
    if (!IsDebuggerPresent())
#endif
    {
      g_mouse_hook = SetWindowsHookExW(
        WH_MOUSE_LL, &mouse_hook_proc, g_instance, 0);
      if (!g_mouse_hook)
        error("Hooking mouse failed");
    }
  }

  int get_vk_by_key(Key key) {
    switch (key) {
      case Key::ButtonLeft: return VK_LBUTTON;
      case Key::ButtonRight: return VK_RBUTTON;
      case Key::ButtonMiddle: return VK_MBUTTON;
      case Key::ButtonBack: return VK_XBUTTON1;
      case Key::ButtonForward: return VK_XBUTTON2;
      default:
         return MapVirtualKeyA(*key, MAPVK_VSC_TO_VK_EX);
    }
  }

  bool ServerStateImpl::on_validate_key_is_down(Key key) {
    if (g_devices.initialized())
      return true;
    return (GetAsyncKeyState(get_vk_by_key(key)) & 0x8000) != 0;
  }

  bool listen_for_client() {
    auto socket = g_state.listen_for_client_connections();
    return (socket && 
      WSAAsyncSelect(*socket, g_window,
        WM_APP_CLIENT_MESSAGE, FD_ACCEPT) == 0);
  }

  bool accept_client() {
    auto socket = g_state.accept_client_connection();
    return (socket && 
      WSAAsyncSelect(*socket, g_window,
        WM_APP_CLIENT_MESSAGE, (FD_READ | FD_CLOSE)) == 0);
  }

  void apply_updates() {
    // reinsert hook in front of callchain
    hook_devices();
  }

  LRESULT CALLBACK window_proc(HWND window, UINT message,
      WPARAM wparam, LPARAM lparam) {
    switch(message) {
      case WM_DESTROY:
        g_devices.shutdown();
        ::PostQuitMessage(0);
        return 0;

      case WM_APP_CLIENT_MESSAGE:
        if (lparam == FD_ACCEPT) {
          accept_client();
        }
        else if (lparam == FD_READ) {
          if (g_state.read_client_messages(Duration::zero())) {
            apply_updates();
          }
          else {
            g_state.disconnect();
          }
        }
        else {
          verbose("Connection to keymapper lost");
          verbose("---------------");
          g_state.reset_configuration();
          unhook_devices();
        }
        return 0;

      case WM_INPUT_DEVICE_CHANGE: {
        const auto device = reinterpret_cast<HANDLE>(lparam);
        if (wparam == GIDC_ARRIVAL)
          g_devices.on_device_attached(device);
        if (wparam == GIDC_REMOVAL)
          g_devices.on_device_removed(device);
        g_state.set_device_descs(g_devices.device_descs());
        break;
      }

      case WM_APP_DEVICE_INPUT: {
        const auto& event = *reinterpret_cast<const KeyEvent*>(wparam);
        const auto device = reinterpret_cast<HANDLE>(lparam);
        const auto device_index = g_devices.get_device_index(device);
        if (device_index >= 0 &&
            g_state.translate_input(event, device_index)) {
          if (!g_state.flush_scheduled_at())
            g_state.flush_send_buffer();
          return 1;
        }
        return 0;
      }

      case WM_TIMER: {
        if (wparam == TIMER_FLUSH_SEND_BUFFER) {
          KillTimer(g_window, TIMER_FLUSH_SEND_BUFFER);
          g_state.flush_send_buffer();
        }
        else if (wparam == TIMER_TIMEOUT) {
          const auto timeout = make_input_timeout_event(g_state.timeout());
          g_state.cancel_timeout();
          g_state.translate_input(timeout, Stage::any_device_index);
          if (!g_state.flush_scheduled_at())
            g_state.flush_send_buffer();
        }
        break;
      }
    }
    return DefWindowProcW(window, message, wparam, lparam);
  }

  void show_notification(const char* message) {
    MessageBoxA(nullptr, message, "Keymapper",
      MB_ICONWARNING | MB_TOPMOST);
  }
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
  auto settings = Settings{ };
  if (!interpret_commandline(settings, __argc, __wargv)) {
    print_help_message();
    return 1;
  }
  g_show_notification = &show_notification;

  const auto single_instance = LimitSingleInstance(
    "Global\\{E28F6E4E-A892-47ED-A6C2-DAC6AB8CCBFC}");
  if (single_instance.is_another_instance_running()) {
    error("Another instance is already running");
    return 1;
  }

  g_state.reset_configuration();

  SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
  g_instance = instance;
  g_verbose_output = settings.verbose;

  const auto window_class_name = L"keymapperd";
  auto window_class = WNDCLASSEXW{ };
  window_class.cbSize = sizeof(WNDCLASSEXW);
  window_class.lpfnWndProc = &window_proc;
  window_class.hInstance = instance;
  window_class.lpszClassName = window_class_name;
  if (!RegisterClassExW(&window_class))
    return 1;

  g_window = CreateWindowExW(0, window_class_name, NULL, 0,
    CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
    HWND_MESSAGE, NULL, NULL,  NULL);
  
  g_devices.initialize(g_window, WM_APP_DEVICE_INPUT);

  auto disable = BOOL{ FALSE };
  SetUserObjectInformationA(GetCurrentProcess(),
    UOI_TIMERPROC_EXCEPTION_SUPPRESSION, &disable, sizeof(disable));

  if (!listen_for_client())
    return 1;

  auto message = MSG{ };
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
  verbose("Exiting");
  return 0;
}
