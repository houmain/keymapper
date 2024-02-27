
#include "server/Settings.h"
#include "server/ClientPort.h"
#include "server/ButtonDebouncer.h"
#include "server/verbose_debug_io.h"
#include "runtime/Stage.h"
#include "runtime/Timeout.h"
#include "common/windows/LimitSingleInstance.h"
#include "common/output.h"
#include "Devices.h"
#include <WinSock2.h>

namespace {
  // Calling SendInput directly from mouse hook proc seems to trigger a
  // timeout, therefore it is called after returning from the hook proc. 
  // But for keyboard input it is still more reliable to call it directly!
  const auto TIMER_FLUSH_SEND_BUFFER = 1;
  const auto TIMER_TIMEOUT = 2;
  const auto WM_APP_CLIENT_MESSAGE = WM_APP + 0;
  const auto WM_APP_DEVICE_INPUT = WM_APP + 1;
  const auto injected_ident = ULONG_PTR(0xADDED);
  const auto no_device_index = -1;

  HINSTANCE g_instance;
  HWND g_window;
  ClientPort g_client;
  std::optional<ButtonDebouncer> g_button_debouncer;
  std::unique_ptr<Stage> g_stage;
  std::unique_ptr<Stage> g_new_stage;
  const std::vector<int>* g_new_active_contexts;
  HHOOK g_keyboard_hook;
  HHOOK g_mouse_hook;
  bool g_sending_key;
  std::vector<KeyEvent> g_send_buffer;
  std::vector<Key> g_buttons_down;
  bool g_flush_scheduled;
  KeyEvent g_last_key_event;
  std::chrono::milliseconds g_timeout_ms;
  std::optional<Clock::time_point> g_timeout_start_at;
  bool g_cancel_timeout_on_up;
  std::vector<Key> g_virtual_keys_down;
  Devices g_devices;

  void apply_updates();
  bool translate_input(KeyEvent input, int device_index = no_device_index);

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

  INPUT make_key_input(const KeyEvent& event) {
    auto key = INPUT{ };
    key.type = INPUT_KEYBOARD;
    key.ki.dwExtraInfo = injected_ident;
    key.ki.dwFlags |= (event.state == KeyState::Up ? KEYEVENTF_KEYUP : 0);
    key.ki.time = GetTickCount();

    switch (event.key) {
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

    const auto it = std::find(g_buttons_down.begin(), g_buttons_down.begin(), event.key);
    if (event.state == KeyState::Down) {
      if (it != g_buttons_down.end())
        return true;
      g_buttons_down.push_back(event.key);
    }
    else if (it != g_buttons_down.end()) {
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
    switch (static_cast<Key>(event.key)) {
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
      default:
        return std::nullopt;
    }
    return button;
  }

  bool is_control_up(const KeyEvent& event) {
    return (event.state == KeyState::Up &&
          (event.key == Key::ControlLeft ||
           event.key == Key::ControlRight));
  }

  void schedule_flush(Duration delay = { }) {
    if (g_flush_scheduled)
      return;
    g_flush_scheduled = true;
    SetTimer(g_window, TIMER_FLUSH_SEND_BUFFER,
      static_cast<UINT>(std::chrono::duration_cast<
        std::chrono::milliseconds>(delay).count()), 
      nullptr);
  }

  void toggle_virtual_key(Key key) {
    const auto it = std::find(g_virtual_keys_down.begin(), g_virtual_keys_down.end(), key);
    if (it == g_virtual_keys_down.end()) {
      g_virtual_keys_down.push_back(key);
      translate_input({ key, KeyState::Down });
    }
    else {
      g_virtual_keys_down.erase(it);
      translate_input({ key, KeyState::Up });
    }
  }

  void flush_send_buffer() {
    if (g_sending_key)
      return;
    g_sending_key = true;
    g_flush_scheduled = false;

    auto i = size_t{ };
    for (; i < g_send_buffer.size(); ++i) {
      const auto& event = g_send_buffer[i];
      const auto is_first = (i == 0);
      const auto is_last = (i == g_send_buffer.size() - 1);

      if (is_action_key(event.key)) {
        if (event.state == KeyState::Down)
          g_client.send_triggered_action(
            static_cast<int>(*event.key - *Key::first_action));
        continue;
      }

      if (is_any_virtual_key(event.key)) {
        if (event.state == KeyState::Down)
          toggle_virtual_key(event.key);
        continue;
      }

      if (event.key == Key::timeout) {
        schedule_flush(timeout_to_milliseconds(event.timeout));
        ++i;
        break;
      }

      // do not release Control too quickly
      // otherwise copy/paste does not work in some input fields
      if (!is_first && is_control_up(event)) {
        schedule_flush(std::chrono::milliseconds(10));
        break;
      }

      if (prevent_button_repeat(event))
        continue;

      if (g_button_debouncer && event.state == KeyState::Down) {
        const auto delay = g_button_debouncer->on_key_down(event.key, !is_last);
        if (delay != Duration::zero()) {
          schedule_flush(delay);
          break;
        }
      }

      if (g_devices.initialized()) {
        g_devices.send_input(event);
      }
      else{
        auto input = make_button_input(event);
        if (!input.has_value())
          input = make_key_input(event);
        ::SendInput(1, &input.value(), sizeof(INPUT));
      }
    }
    g_send_buffer.erase(g_send_buffer.begin(), g_send_buffer.begin() + i);
    g_sending_key = false;
  }

  void schedule_timeout(std::chrono::milliseconds timeout, bool cancel_on_up) {
    g_timeout_ms = timeout;
    g_timeout_start_at = Clock::now();
    g_cancel_timeout_on_up = cancel_on_up;
    SetTimer(g_window, TIMER_TIMEOUT, 
      static_cast<UINT>(timeout.count()),  nullptr);
  }

  void cancel_timeout() {
    KillTimer(g_window, TIMER_TIMEOUT);
    g_timeout_start_at.reset();
  }

  void send_key_sequence(const KeySequence& key_sequence) {
    for (const auto& event : key_sequence)
      g_send_buffer.push_back(event);
  }

  bool translate_input(KeyEvent input, int device_index) {
    // ignore key repeat while a flush or a timeout is pending
    if (input == g_last_key_event && 
          (g_flush_scheduled || g_timeout_start_at)) {
      verbose_debug_io(input, { }, true);
      return true;
    }

    auto cancelled_timeout = false;
    if (g_timeout_start_at &&
        (input.state == KeyState::Down || g_cancel_timeout_on_up)) {
      // cancel current time out, inject event with elapsed time
      const auto time_since_timeout_start = 
        (Clock::now() - *g_timeout_start_at);
      cancel_timeout();
      translate_input(make_input_timeout_event(time_since_timeout_start), device_index);
      cancelled_timeout = true;
    }

    // turn NumLock succeeding Pause into another Pause
    auto translated_numlock_to_pause = false;
    if (input.state == g_last_key_event.state && 
        input.key == Key::NumLock && 
        g_last_key_event.key == Key::Pause) {
      input.key = Key::Pause;
      translated_numlock_to_pause = true;
    }
    if (input.key != Key::timeout)
      g_last_key_event = input;

    apply_updates();

    auto output = g_stage->update(input, device_index);

    if (g_stage->should_exit()) {
      verbose("Read exit sequence");
      ::PostQuitMessage(0);
      return true;
    }

    // waiting for input timeout
    if (!output.empty() && output.back().key == Key::timeout) {
      const auto& request = output.back();
      schedule_timeout(
        timeout_to_milliseconds(request.timeout), 
        cancel_timeout_on_up(request.state));
      output.pop_back();
    }

    const auto translated =
        output.size() != 1 ||
        output.front().key != input.key ||
        (output.front().state == KeyState::Up) != (input.state == KeyState::Up) ||
        translated_numlock_to_pause;

    const auto intercept_and_send =
        g_flush_scheduled ||
        cancelled_timeout ||
        translated ||
        // always intercept and send AltGr
        input.key == Key::AltRight;

    verbose_debug_io(input, output, intercept_and_send);

    if (intercept_and_send)
      send_key_sequence(output);

    g_stage->reuse_buffer(std::move(output));
    return intercept_and_send;
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
        return !g_sending_key;

      return false;
    }

    return translate_input(input);
  }

  LRESULT CALLBACK keyboard_hook_proc(int code, WPARAM wparam, LPARAM lparam) {
    if (code == HC_ACTION) {
      const auto& kbd = *reinterpret_cast<const KBDLLHOOKSTRUCT*>(lparam);
      if (translate_keyboard_input(wparam, kbd)) {
        if (!g_flush_scheduled)
          flush_send_buffer();
        return -1;
      }
    }
    return CallNextHookEx(g_keyboard_hook, code, wparam, lparam);
  }

  void evaluate_device_filters() {
    if (!g_stage->has_device_filters())
      return;
    verbose("Evaluating device filters");
    g_stage->evaluate_device_filters(g_devices.device_names());
  }

  std::optional<KeyEvent> get_button_event(WPARAM wparam, const MSLLHOOKSTRUCT& ms) {
    auto state = KeyState::Down;
    if (g_button_debouncer)
      g_button_debouncer->on_mouse_move(ms.pt.x, ms.pt.y);
    auto key = Key::none;
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
      default:
        return { };
    }
    return KeyEvent{ key, state };
  }

  bool translate_mouse_input(WPARAM wparam, const MSLLHOOKSTRUCT& ms) {
    const auto injected = (ms.dwExtraInfo == injected_ident);
    if (injected || g_sending_key)
      return false;
    
    const auto input = get_button_event(wparam, ms);
    if (!input.has_value())
      return false;

    return translate_input(*input);
  }

  LRESULT CALLBACK mouse_hook_proc(int code, WPARAM wparam, LPARAM lparam) {
    if (code == HC_ACTION) {
      const auto& ms = *reinterpret_cast<const MSLLHOOKSTRUCT*>(lparam);
      if (translate_mouse_input(wparam, ms)) {
        schedule_flush();
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

    verbose("Hooking devices");

    g_keyboard_hook = SetWindowsHookExW(
      WH_KEYBOARD_LL, &keyboard_hook_proc, g_instance, 0);
    if (!g_keyboard_hook)
      error("Hooking keyboard failed");

    g_mouse_hook = SetWindowsHookExW(
      WH_MOUSE_LL, &mouse_hook_proc, g_instance, 0);
    if (!g_mouse_hook)
      error("Hooking mouse failed");
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

  void validate_state() {
    verbose("Validating state");
    g_stage->validate_state([](Key key) {
      return (GetAsyncKeyState(get_vk_by_key(key)) & 0x8000) != 0;
    });
  }

  bool accept() {
    if (!g_client.accept() ||
        WSAAsyncSelect(g_client.socket(), g_window,
          WM_APP_CLIENT_MESSAGE, (FD_READ | FD_CLOSE)) != 0) {
      error("Connecting to keymapper failed");
      return false;
    }
    verbose("Keymapper connected");
    return true;
  }

  bool handle_client_message() {
    return g_client.read_messages(Duration::zero(), [&](Deserializer& d) {
      const auto message_type = d.read<MessageType>();
      if (message_type == MessageType::active_contexts) {
        g_new_active_contexts = 
          &g_client.read_active_contexts(d);
      }
      else if (message_type == MessageType::validate_state) {
        validate_state();
      }
      else if (message_type == MessageType::configuration) {
        g_new_stage = g_client.read_config(d);
        if (!g_new_stage)
          return error("Receiving configuration failed");

        verbose("Configuration received");
      }
    });
  }

  void release_all_keys() {
    verbose("Releasing all keys");
    for (auto key : g_stage->get_physical_keys_down())
      g_send_buffer.push_back(KeyEvent(key, KeyState::Up));
  }

  void set_active_contexts(const std::vector<int>& indices) {
    auto output = g_stage->set_active_contexts(indices);
    send_key_sequence(output);
    g_stage->reuse_buffer(std::move(output));
    if (!g_flush_scheduled)
      flush_send_buffer();
  }

  void apply_updates() {
    if (g_new_stage) {
      release_all_keys();
      g_stage = std::move(g_new_stage);
      g_virtual_keys_down.clear();
      evaluate_device_filters();
      flush_send_buffer();

      if (g_stage->has_device_filters())
        g_devices.initialize(g_window, WM_APP_DEVICE_INPUT);
    }

    if (g_new_active_contexts) {
      set_active_contexts(*g_new_active_contexts);
      g_new_active_contexts = nullptr;

      // reinsert hook in front of callchain
      hook_devices();
    }
  }

  LRESULT CALLBACK window_proc(HWND window, UINT message,
      WPARAM wparam, LPARAM lparam) {
    switch(message) {
      case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

      case WM_APP_CLIENT_MESSAGE:
        if (lparam == FD_ACCEPT) {
          accept();
        }
        else if (lparam == FD_READ) {
          if (handle_client_message()) {
            apply_updates();
          }
          else {
            g_client.disconnect();
          }
        }
        else {
          verbose("Connection to keymapper lost");
          verbose("---------------");
          unhook_devices();
        }
        return 0;

      case WM_INPUT_DEVICE_CHANGE: {
        const auto device = reinterpret_cast<HANDLE>(lparam);
        if (wparam == GIDC_ARRIVAL)
          g_devices.on_device_attached(device);
        verbose("Device '%s' %s", 
          g_devices.get_device_name(device).c_str(),
          (wparam == GIDC_ARRIVAL ? "attached" : "removed"));
        if (wparam == GIDC_REMOVAL)
          g_devices.on_device_removed(device);
        evaluate_device_filters();
        break;
      }

      case WM_APP_DEVICE_INPUT: {
        const auto event = KeyEvent{ 
          static_cast<Key>(LOWORD(wparam)), 
          static_cast<KeyState>(HIWORD(wparam)) 
        };
        const auto device = reinterpret_cast<HANDLE>(lparam);
        const auto device_index = g_devices.get_device_index(device);
        if (translate_input(event, device_index)) {
          if (!g_flush_scheduled)
            flush_send_buffer();
          return 1;
        }
        return 0;
      }

      case WM_TIMER: {
        if (wparam == TIMER_FLUSH_SEND_BUFFER) {
          KillTimer(g_window, TIMER_FLUSH_SEND_BUFFER);
          flush_send_buffer();
        }
        else if (wparam == TIMER_TIMEOUT) {
          cancel_timeout();
          translate_input(make_input_timeout_event(g_timeout_ms));
          if (!g_flush_scheduled)
            flush_send_buffer();
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

  SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
  g_instance = instance;
  g_verbose_output = settings.verbose;
  if (settings.debounce)
    g_button_debouncer.emplace();
  g_stage = std::make_unique<Stage>(std::vector<Stage::Context>{ });

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
  
  auto disable = BOOL{ FALSE };
  SetUserObjectInformationA(GetCurrentProcess(),
    UOI_TIMERPROC_EXCEPTION_SUPPRESSION, &disable, sizeof(disable));

  if (!g_client.initialize() ||
      WSAAsyncSelect(g_client.listen_socket(), g_window,
        WM_APP_CLIENT_MESSAGE, FD_ACCEPT) != 0) {
    error("Initializing keymapper connection failed");
    return 1;
  }

  verbose("Entering update loop");
  auto message = MSG{ };
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
  verbose("Exiting");
  return 0;
}
