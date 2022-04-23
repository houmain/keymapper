
#include "FocusedWindow.h"
#include "client/Settings.h"
#include "client/ConfigFile.h"
#include "runtime/Stage.h"
#include "common/windows/LimitSingleInstance.h"
#include "common/common.h"
#include "config/Key.h"
#include "Wtsapi32.h"
#include <array>
#include <optional>
#include <cstdarg>

const auto ControlRightPrecedingAltGr = 0x21D;

const auto window_class_name = L"Keymapper";

const auto config_filename = L"keymapper.conf";
const int  update_interval_ms = 50;
const int  update_configuration_rate = 10;

namespace {
  Settings g_settings;
  std::optional<ConfigFile> g_config_file;
  FocusedWindowPtr g_focused_window;
  std::unique_ptr<Stage> g_stage;
  std::vector<int> g_new_active_contexts;
  std::vector<int> g_current_active_contexts;
  bool g_was_inaccessible;
  unsigned int g_update_configuration_count;

  HINSTANCE g_instance;
  HHOOK g_keyboard_hook;
  bool g_sending_key;
  std::vector<INPUT> g_send_buffer;
  bool g_session_changed;
  bool g_output_on_release;
  
  std::wstring utf8_to_wide(const std::string& str) {
    auto result = std::wstring();
    result.resize(MultiByteToWideChar(CP_UTF8, 0, 
      str.data(), static_cast<int>(str.size()), 
      NULL, 0));
    MultiByteToWideChar(CP_UTF8, 0, 
      str.data(), static_cast<int>(str.size()), 
      result.data(), static_cast<int>(result.size()));
    return result;
  }

  bool starts_with_case_insensitive(const std::string& string, const char* value) {
    for (const auto* a = string.c_str(), *b = value; *b != '\0'; ++a, ++b)
      if (*a == '\0' || 
          std::tolower(static_cast<int>(*a)) != std::tolower(static_cast<int>(*b)))
        return false;
    return true;
  }

  bool execute_terminal_command(const std::string& command) {
    auto cmd = std::wstring(MAX_PATH, ' ');
    cmd.resize(GetSystemDirectoryW(cmd.data(), static_cast<UINT>(cmd.size())));
    cmd += L"\\CMD.EXE";

    auto args = L"/C " + utf8_to_wide(command);

    auto flags = DWORD{ };
    if (!starts_with_case_insensitive(command, "cmd") && 
        !starts_with_case_insensitive(command, "powershell"))
      flags |= CREATE_NO_WINDOW;

    auto startup_info = STARTUPINFOW{ sizeof(STARTUPINFOW) };
    auto process_info = PROCESS_INFORMATION{ };
    if (!CreateProcessW(cmd.data(), args.data(), nullptr, nullptr, FALSE, 
        flags, nullptr, nullptr, &startup_info, &process_info)) 
      return false;
    
    CloseHandle(process_info.hProcess);
    CloseHandle(process_info.hThread);
    return true;
  }

  void update_active_contexts() {
    const auto& contexts = g_config_file->config().contexts;
    const auto& window_class = get_class(*g_focused_window);
    const auto& window_title = get_title(*g_focused_window);

    g_new_active_contexts.clear();
    for (auto i = 0; i < static_cast<int>(contexts.size()); ++i)
      if (contexts[i].matches(window_class, window_title))
        g_new_active_contexts.push_back(i);

    if (g_new_active_contexts != g_current_active_contexts) {
      verbose("Active contexts updated (%u)", g_new_active_contexts.size());
      g_stage->set_active_contexts(g_new_active_contexts);
      g_current_active_contexts.swap(g_new_active_contexts);
    }
  }
} // namespace

void reset_state() {
  const auto& config = g_config_file->config();

  auto contexts = std::vector<Stage::Context>();
  for (auto& config_context : config.contexts) {
    auto& context = contexts.emplace_back();
    for (const auto& input : config_context.inputs)
      context.inputs.push_back({ std::move(input.input), input.output_index });
    context.outputs = std::move(config_context.outputs);
    for (const auto& output : config_context.command_outputs)
      context.command_outputs.push_back({ std::move(output.output), output.index });
  }
  g_stage = std::make_unique<Stage>(std::move(contexts));
  g_focused_window = create_focused_window();
  update_active_contexts();
}

void execute_action(int triggered_action) {
  if (triggered_action >= 0 &&
      triggered_action < static_cast<int>(g_config_file->config().actions.size())) {
    const auto& action = g_config_file->config().actions[triggered_action];
    const auto& command = action.terminal_command;
    verbose("Executing terminal command '%s'", command.c_str());
    execute_terminal_command(command);
  }
}

void update_configuration() {
  if (!g_settings.auto_update_config)
    return;
  if (g_stage->is_output_down())
    return;
  if (g_update_configuration_count++ % update_configuration_rate)
    return;

  if (g_config_file->update()) {
    verbose("Configuration updated");
    reset_state();

    g_current_active_contexts.clear();
    update_active_contexts();
  }
}

void validate_state(bool check_accessibility) {

  // validate internal state when a window of another user was focused
  if (check_accessibility) {
    if (is_inaccessible(*g_focused_window)) {
      g_was_inaccessible = true;
      return;
    }
    if (!std::exchange(g_was_inaccessible, false))
      return;
  }

  g_stage->validate_state([](KeyCode keycode) {
    const auto vk = MapVirtualKeyA(keycode, MAPVK_VSC_TO_VK_EX);
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
  });
}

bool update_focused_window() {
  if (!update_focused_window(*g_focused_window))
    return false;

  verbose("Detected focused window changed:");
  verbose("  class = '%s'", get_class(*g_focused_window).c_str());
  verbose("  title = '%s'", get_title(*g_focused_window).c_str());
  update_active_contexts();
  return true;
}

KeySequence apply_input(KeyEvent event) {
  auto output = g_stage->update(event);
  if (g_stage->should_exit()) {
    verbose("Read exit sequence");
    ::PostQuitMessage(0);
  }
  return output;
}

void reuse_buffer(KeySequence&& buffer) {
  g_stage->reuse_buffer(std::move(buffer));
}

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
        execute_action(static_cast<int>(event.key - first_action_key));
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
  const auto injected = (kbd.flags & LLKHF_INJECTED);
  if (!kbd.scanCode || injected || g_sending_key)
    return false;

  const auto input = get_key_event(wparam, kbd);

  // intercept ControlRight preceding AltGr
  if (input.key == ControlRightPrecedingAltGr)
    return true;

  // after OutputOnRelease block input until trigger is released
  if (g_output_on_release) {
    if (input.state != KeyState::Up)
      return true;
    g_output_on_release = false;
  }

  auto output = apply_input(input);

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
    
  reuse_buffer(std::move(output));
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

      if (update_focused_window()) {
        // validate state when window was inaccessible
        // force validation after session change
        const auto check_accessibility = 
          !std::exchange(g_session_changed, false);
        validate_state(check_accessibility);

        // reinsert hook in front of callchain
        unhook_keyboard();
        if (!hook_keyboard())
          verbose("Resetting keyboard hook failed");
      }
      break;
    }
  }
  return DefWindowProcW(window, message, wparam, lparam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
  g_settings.config_file_path = config_filename;

  if (!interpret_commandline(g_settings, __argc, __wargv)) {
    print_help_message();
    return 1;
  }

  g_verbose_output = g_settings.verbose;
  g_output_color = !g_settings.no_color;

  LimitSingleInstance single_instance("Global\\{658914E7-CCA6-4425-89FF-EF4A13B75F31}");
  if (single_instance.is_another_instance_running()) {
    error("Another instance is already running");
    return 1;
  }

  SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

  verbose("Loading configuration file '%ws'", g_settings.config_file_path.c_str());
  g_config_file.emplace(g_settings.config_file_path);
  if (!g_config_file->update())
    return 1;
  reset_state();

  g_instance = instance;

  auto window_class = WNDCLASSEXW{ };
  window_class.cbSize = sizeof(WNDCLASSEXW);
  window_class.lpfnWndProc = &window_proc;
  window_class.hInstance = instance;
  window_class.lpszClassName = window_class_name;
  if (!RegisterClassExW(&window_class))
    return 1;

  verbose("Hooking keyboard");
  if (!hook_keyboard()) {
    error("Hooking keyboard failed");
    UnregisterClassW(window_class_name, instance);
    return 1;
  }

  auto window = CreateWindowExW(0, window_class_name, NULL, 0,
    CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
    HWND_MESSAGE, NULL, NULL,  NULL);

  WTSRegisterSessionNotification(window, NOTIFY_FOR_THIS_SESSION);
  SetTimer(window, 1, update_interval_ms, NULL);

  verbose("Entering update loop");
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
