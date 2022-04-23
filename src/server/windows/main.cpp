
#include "server/Settings.h"
#include "server/ClientPort.h"
#include "runtime/Stage.h"
#include "config/Key.h"
#include "common/windows/LimitSingleInstance.h"
#include "common/output.h"
#include <WinSock2.h>

#if !defined(NDEBUG)
# include "config/Key.cpp"

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

namespace {
  const auto WM_APP_CLIENT_MESSAGE = WM_APP + 0;
  const auto injected_ident = ULONG_PTR(0xADDED);

  HINSTANCE g_instance;
  HWND g_window;
  std::unique_ptr<Stage> g_stage;
  std::unique_ptr<ClientPort> g_client;
  std::unique_ptr<Stage> g_new_stage;
  const std::vector<int>* g_new_active_contexts;
  HHOOK g_keyboard_hook;
  bool g_sending_key;
  std::vector<INPUT> g_send_buffer;
  bool g_output_on_release;

  void apply_updates();

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

    // special handling of ShiftRight
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

  void flush_send_buffer() {
    g_sending_key = true;
    const auto sent = ::SendInput(
      static_cast<UINT>(g_send_buffer.size()),
      g_send_buffer.data(), sizeof(INPUT));
    g_send_buffer.clear();
    g_sending_key = false;
  }

  bool is_control(const KeyEvent& event) {
    return (static_cast<Key>(event.key) == Key::ControlLeft ||
            static_cast<Key>(event.key) == Key::ControlRight);
  }

  void send_key_sequence(const KeySequence& key_sequence) {
    for (const auto& event : key_sequence) {
      if (event.state == KeyState::OutputOnRelease) {
        flush_send_buffer();
        g_output_on_release = true;
      }
      else if (is_action_key(event.key)) {
        if (event.state == KeyState::Down)
          g_client->send_triggered_action(
            static_cast<int>(event.key - first_action_key));
      }
      else {
        // workaround: do not release Control too quickly
        // otherwise copy/paste does not work in some input fields
        if (!g_send_buffer.empty() &&
            !g_output_on_release &&
            event.state == KeyState::Up &&
            is_control(event)) {
          flush_send_buffer();
          Sleep(1);
        }
        send_event(event);
      }
    }

    if (!g_output_on_release)
      flush_send_buffer();
  }

  bool translate_keyboard_input(WPARAM wparam, const KBDLLHOOKSTRUCT& kbd) {
    const auto injected = (kbd.dwExtraInfo == injected_ident);
    if (!kbd.scanCode || injected || g_sending_key)
      return false;

    const auto input = get_key_event(wparam, kbd);

    // intercept ControlRight preceding AltGr
    const auto ControlRightPrecedingAltGr = 0x21D;
    if (input.key == ControlRightPrecedingAltGr)
      return true;

    // after OutputOnRelease block input until trigger is released
    if (g_output_on_release) {
      if (input.state != KeyState::Up)
        return true;
      g_output_on_release = false;
    }

    apply_updates();

    auto output = g_stage->update(input);
    if (g_stage->should_exit()) {
      verbose("Read exit sequence");
      ::PostQuitMessage(0);
      return true;
    }

    const auto translated =
        (output.size() != 1 ||
        output.front().key != input.key ||
        (output.front().state == KeyState::Up) != (input.state == KeyState::Up) ||
        // always intercept and send AltGr
        input.key == *Key::AltRight);

  #if !defined(NDEBUG)
    verbose(translated ? "%s--> %s" : "%s",
      format(input).c_str(), format(output).c_str());
  #endif

    if (translated)
      send_key_sequence(output);

    g_stage->reuse_buffer(std::move(output));
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
  
  void unhook_keyboard() {
    if (g_keyboard_hook)
      UnhookWindowsHookEx(g_keyboard_hook);
    g_keyboard_hook = nullptr;
  }

  bool hook_keyboard() {
    unhook_keyboard();
    g_keyboard_hook = SetWindowsHookExW(
      WH_KEYBOARD_LL, &keyboard_hook_proc, g_instance, 0);
    return (g_keyboard_hook != nullptr);
  }

  void validate_state() {
    verbose("Validating state");
    g_stage->validate_state([](KeyCode keycode) {
      const auto vk = MapVirtualKeyA(keycode, MAPVK_VSC_TO_VK_EX);
      return (GetAsyncKeyState(vk) & 0x8000) != 0;
    });
  }

  bool accept() {
    if (!g_client->accept() ||
        WSAAsyncSelect(g_client->socket(), g_window,
          WM_APP_CLIENT_MESSAGE, (FD_READ | FD_CLOSE)) != 0) {
      error("Connecting to keymapper failed");
      return false;
    }
    verbose("Keymapper connected");
    return true;
  }

  void handle_client_message() {
    g_client->read_messages(0, [&](Deserializer& d) {
      const auto message_type = d.read<MessageType>();
      if (message_type == MessageType::active_contexts) {
        g_new_active_contexts = 
          &g_client->read_active_contexts(d);
      }
      else if (message_type == MessageType::validate_state) {
        validate_state();
      }
      else if (message_type == MessageType::configuration) {
        g_new_stage = g_client->read_config(d);
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
      verbose("Updating hooks");
      hook_keyboard();
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
          handle_client_message();
          apply_updates();
        }
        else {
          verbose("Connection to keymapper lost");
          verbose("---------------");
          unhook_keyboard();
        }
        return 0;

      case WM_TIMER:
        return 0;
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

  g_client = std::make_unique<ClientPort>();
  if (!g_client->initialize() ||
      WSAAsyncSelect(g_client->listen_socket(), g_window,
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
