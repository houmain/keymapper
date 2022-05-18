
#include "server/Settings.h"
#include "server/ClientPort.h"
#include "server/ButtonDebouncer.h"
#include "server/verbose_debug_io.h"
#include "runtime/Stage.h"
#include "runtime/Key.h"
#include "common/windows/LimitSingleInstance.h"
#include "common/output.h"
#include <WinSock2.h>

namespace {
  // Calling SendMessage directly from mouse hook proc seems to trigger a
  // timeout, therefore it is called after returning from the hook proc. 
  // But for keyboard input it is still more reliable to call it directly!
  const auto TIMER_FLUSH_SEND_BUFFER = 1;
  const auto WM_APP_CLIENT_MESSAGE = WM_APP + 0;
  const auto injected_ident = ULONG_PTR(0xADDED);

  HINSTANCE g_instance;
  HWND g_window;
  ClientPort g_client;
  ButtonDebouncer g_button_debouncer;
  std::unique_ptr<Stage> g_stage;
  std::unique_ptr<Stage> g_new_stage;
  const std::vector<int>* g_new_active_contexts;
  HHOOK g_keyboard_hook;
  HHOOK g_mouse_hook;
  bool g_sending_key;
  std::vector<KeyEvent> g_send_buffer;
  std::vector<KeyEvent> g_send_buffer_on_release;
  bool g_output_on_release;
  bool g_flush_scheduled;
  KeyEvent g_last_key_event;

  void apply_updates();

  KeyEvent get_key_event(WPARAM wparam, const KBDLLHOOKSTRUCT& kbd) {
    auto key_code = (kbd.scanCode |
      (kbd.flags & LLKHF_EXTENDED ? 0xE000 : 0));

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

    // special handling of ShiftRight
    if (event.key == Key::ShiftRight) {
      key.ki.wVk = VK_RSHIFT;
    }
    else {
      key.ki.dwFlags |= KEYEVENTF_SCANCODE;
      if (*event.key & 0xE000)
        key.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
      key.ki.wScan = static_cast<WORD>(event.key);
    }
    return key;
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

  void flush_send_buffer() {
    if (g_sending_key)
      return;
    g_sending_key = true;
    g_flush_scheduled = false;

    auto i = 0;
    for (; i < g_send_buffer.size(); ++i) {
      const auto& event = g_send_buffer[i];
      const auto is_first = (i == 0);
      const auto is_last = (i == g_send_buffer.size() - 1);

      // do not release Control too quickly
      // otherwise copy/paste does not work in some input fields
      if (!is_first && is_control_up(event)) {  
        schedule_flush(std::chrono::milliseconds(10));
        break;
      }

      if (event.state == KeyState::Down) {
        const auto delay = g_button_debouncer.on_key_down(event.key, !is_last);
        if (delay.count() > 0) {
          schedule_flush(delay);
          break;
        }
      }

      auto input = make_button_input(event);
      if (!input.has_value())
        input = make_key_input(event);
      ::SendInput(1, &input.value(), sizeof(INPUT));
    }
    g_send_buffer.erase(g_send_buffer.begin(), g_send_buffer.begin() + i);
    g_sending_key = false;
  }

  void send_key_sequence(const KeySequence& key_sequence) {
    auto* send_buffer = &g_send_buffer;
    for (const auto& event : key_sequence)
      if (event.state == KeyState::OutputOnRelease) {
        send_buffer = &g_send_buffer_on_release;
        g_output_on_release = true;
      }
      else if (is_action_key(event.key)) {
        if (event.state == KeyState::Down)
          g_client.send_triggered_action(
            static_cast<int>(*event.key - *Key::first_action));
      }
      else {
        send_buffer->push_back(event);
      }
  }

  bool translate_input(const KeyEvent& input) {
    // ignore key repeat while a flush is pending    
    if (g_flush_scheduled && input == g_last_key_event)
      return true;
    g_last_key_event = input;

    // after OutputOnRelease block input until trigger is released
    if (g_output_on_release) {
      if (input.state != KeyState::Up)
        return true;
      g_send_buffer.insert(g_send_buffer.end(),
        g_send_buffer_on_release.begin(), g_send_buffer_on_release.end());
      g_send_buffer_on_release.clear();
      g_output_on_release = false;
    }

    apply_updates();

    const auto device_index = 0;
    auto output = g_stage->update(input, device_index);

    if (g_stage->should_exit()) {
      verbose("Read exit sequence");
      ::PostQuitMessage(0);
      return true;
    }

    const auto translated =
        output.size() != 1 ||
        output.front().key != input.key ||
        (output.front().state == KeyState::Up) != (input.state == KeyState::Up);

    const auto intercept_and_send =
        g_flush_scheduled ||
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
    if (!kbd.scanCode || injected || g_sending_key)
      return false;

    const auto input = get_key_event(wparam, kbd);

    // intercept ControlRight preceding AltGr
    const auto ControlRightPrecedingAltGr = 0x21D;
    if (*input.key == ControlRightPrecedingAltGr)
      return true;

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
  
  std::optional<KeyEvent> get_button_event(WPARAM wparam, const MSLLHOOKSTRUCT& ms) {
    auto state = KeyState::Down;
    g_button_debouncer.on_mouse_move(ms.pt.x, ms.pt.y);
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
    verbose("Hooking devices");

    g_keyboard_hook = SetWindowsHookExW(
      WH_KEYBOARD_LL, &keyboard_hook_proc, g_instance, 0);
    if (!g_keyboard_hook)
      error("Hooking keyboard failed");

    if (g_stage && g_stage->has_mouse_mappings()) {
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

  void validate_state() {
    verbose("Validating state");
    if (g_stage)
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

  void apply_updates() {
    // do not apply updates while a key is down
    if (g_stage && g_stage->is_output_down())
      return;

    if (g_new_stage)
      g_stage = std::move(g_new_stage);

    if (g_new_active_contexts) {
      g_stage->set_active_contexts(*g_new_active_contexts);
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

      case WM_TIMER: {
        if (wparam == TIMER_FLUSH_SEND_BUFFER) {
          KillTimer(g_window, TIMER_FLUSH_SEND_BUFFER);
          flush_send_buffer();
        }
        break;
      }
    }
    return DefWindowProcW(window, message, wparam, lparam);
  }
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
  auto settings = Settings{ };
  if (!interpret_commandline(settings, __argc, __wargv)) {
    print_help_message();
    return 1;
  }

  const auto single_instance = LimitSingleInstance(
    "Global\\{E28F6E4E-A892-47ED-A6C2-DAC6AB8CCBFC}");
  if (single_instance.is_another_instance_running()) {
    error("Another instance is already running");
    return 1;
  }

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
