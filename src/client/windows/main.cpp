
#include "client/FocusedWindow.h"
#include "client/Settings.h"
#include "client/ConfigFile.h"
#include "client/ServerPort.h"
#include "common/windows/LimitSingleInstance.h"
#include "common/output.h"
#include "Wtsapi32.h"
#include <WinSock2.h>
#include <shellapi.h>
#include <Shlobj.h>

namespace {
  const auto online_help_url = "https://github.com/houmain/keymapper#keymapper";

  const auto WM_APP_RESET = WM_APP + 0;
  const auto WM_APP_SERVER_MESSAGE = WM_APP + 1;
  const auto WM_APP_TRAY_NOTIFY = WM_APP + 2;
  const auto TIMER_UPDATE_CONFIG = 1;
  const auto TIMER_UPDATE_CONTEXT = 2;
  const auto TIMER_CREATE_TRAY_ICON = 3;
  const auto IDI_EXIT = 1;
  const auto IDI_ACTIVE = 2;
  const auto IDI_HELP = 3;
  const auto IDI_OPEN_CONFIG = 4;
  const auto IDI_UPDATE_CONFIG = 5;

  const auto update_context_inverval_ms = 50;
  const auto update_config_interval_ms = 500;
  const auto recreate_tray_icon_interval_ms = 1000;

  Settings g_settings;
  ConfigFile g_config_file;
  ServerPort g_server;
  FocusedWindow g_focused_window;
  std::vector<int> g_new_active_contexts;
  std::vector<int> g_current_active_contexts;
  bool g_was_inaccessible;
  bool g_session_changed;
  HWND g_window;
  NOTIFYICONDATAW g_tray_icon;
  bool g_active{ true };
  
  std::wstring utf8_to_wide(std::string_view str) {
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
    SetForegroundWindow(g_window);

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

  void execute_action(int triggered_action) {
    const auto& actions = g_config_file.config().actions;
    if (triggered_action >= 0 &&
        triggered_action < static_cast<int>(actions.size())) {
      const auto& action = actions[triggered_action];
      const auto& command = action.terminal_command;
      verbose("Executing terminal command '%s'", command.c_str());
      execute_terminal_command(command);
    }
  }
 
  void update_active_contexts(bool force_send) {
    const auto& contexts = g_config_file.config().contexts;

    g_new_active_contexts.clear();
    if (g_active)
      for (auto i = 0; i < static_cast<int>(contexts.size()); ++i)
        if (contexts[i].matches(g_focused_window.window_class(),
            g_focused_window.window_title()))
          g_new_active_contexts.push_back(i);

    if (force_send || g_new_active_contexts != g_current_active_contexts) {
      verbose("Active contexts updated (%u)", g_new_active_contexts.size());
      g_server.send_active_contexts(g_new_active_contexts);
      g_current_active_contexts.swap(g_new_active_contexts);
    }
  }
  
  void validate_state() {
    // validate internal state when a window of another user was focused
    // force validation after session change
    const auto check_accessibility = 
      !std::exchange(g_session_changed, false);
    if (check_accessibility) {
      if (g_focused_window.is_inaccessible()) {
        g_was_inaccessible = true;
        return;
      }
      if (!std::exchange(g_was_inaccessible, false))
        return;
    }
    g_server.send_validate_state();
  }

  void update_context() {
    if (g_focused_window.update()) {
      verbose("Detected focused window changed:");
      verbose("  class = '%s'", g_focused_window.window_class().c_str());
      verbose("  title = '%s'", g_focused_window.window_title().c_str());
      update_active_contexts(false);
    }
  }

  bool send_config() {
    if (!g_server.send_config(g_config_file.config())) {
      error("Sending configuration failed");
      return false;
    }
    update_active_contexts(true);
    return true;
  }

  bool connect() {
    verbose("Connecting to keymapperd");
    if (!g_server.initialize() ||
        WSAAsyncSelect(g_server.socket(), g_window,
          WM_APP_SERVER_MESSAGE, (FD_READ | FD_CLOSE)) != 0) {
      error("Connecting to keymapperd failed");
      return false;
    }
    return true;
  }

  void update_config(bool check_modified = true) {
    if (g_config_file.update(check_modified)) {
      message("Configuration updated");
      send_config();
    }    
  }

  void open_configuration() {
    const auto filename = g_config_file.filename().wstring();

    // create if it does not exist
    if (auto handle = CreateFileW(filename.c_str(), GENERIC_READ, FILE_SHARE_READ, 
          nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0); 
        handle != INVALID_HANDLE_VALUE) {
      CloseHandle(handle);
    }

    SetForegroundWindow(g_window);
    ShellExecuteW(nullptr, L"open", filename.c_str(), 
      nullptr, nullptr, SW_SHOWNORMAL);
  }

  void open_online_help() {
    SetForegroundWindow(g_window);
    ShellExecuteA(nullptr, "open", online_help_url, 
      nullptr, nullptr, SW_SHOWNORMAL);
  }

  void toggle_active() {
    g_active = !g_active;
    update_active_contexts(true);
  }

  void open_tray_menu() {
    auto popup_menu = CreatePopupMenu();
    AppendMenuW(popup_menu, 
      (g_active ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, IDI_ACTIVE, L"Active");
    AppendMenuW(popup_menu, MF_STRING, IDI_OPEN_CONFIG, L"Configuration");
    if (!g_settings.auto_update_config)
      AppendMenuW(popup_menu, MF_STRING, IDI_UPDATE_CONFIG, L"Reload");
    AppendMenuW(popup_menu, MF_STRING, IDI_HELP, L"Help");
    AppendMenuW(popup_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(popup_menu, MF_STRING, IDI_EXIT, L"Exit");
    SetForegroundWindow(g_window);
    auto cursor_pos = POINT{ };
    GetCursorPos(&cursor_pos);
    TrackPopupMenu(popup_menu, 
      TPM_NOANIMATION | TPM_BOTTOMALIGN | TPM_RIGHTALIGN,
      cursor_pos.x, cursor_pos.y, 0, g_window, nullptr);
  }

  LRESULT CALLBACK window_proc(HWND window, UINT message,
      WPARAM wparam, LPARAM lparam) {

    switch(message) {
      case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

      case WM_WTSSESSION_CHANGE:
        g_session_changed = true;
        return 0;

      case WM_APP_TRAY_NOTIFY:
        if (lparam == WM_LBUTTONUP || lparam == WM_RBUTTONUP)
          open_tray_menu();
        else if (lparam == WM_MBUTTONUP)
          toggle_active();
        else if (lparam == 0x405)
          open_configuration();
        return 0;

      case WM_APP_SERVER_MESSAGE:
        if (lparam == FD_READ) {
          auto triggered_action = -1;
          g_server.receive_triggered_action(Duration::zero(), &triggered_action);
          execute_action(triggered_action);
        }
        else {
          verbose("Connection to keymapperd lost");
          verbose("---------------");
          if (!connect() || !send_config())
            PostQuitMessage(1);
        }
        return 0;

      case WM_COMMAND:
        switch (wparam) {
          case IDI_ACTIVE:
            toggle_active();
            return 0;

          case IDI_OPEN_CONFIG:
            open_configuration();
            return 0;

          case IDI_UPDATE_CONFIG:
            update_config(false);
            return 0;

          case IDI_HELP:
            open_online_help();
            return 0;

          case IDI_EXIT:
            PostQuitMessage(0);
            return 0;
        }
        break;

      case WM_TIMER: {
        if (wparam == TIMER_UPDATE_CONTEXT) {
          update_context();
          validate_state();
        }
        else if (wparam == TIMER_UPDATE_CONFIG) {
          update_config();
        }
        else if (wparam == TIMER_CREATE_TRAY_ICON) {
          // workaround: re/create tray icon after taskbar was created
          // RegisterWindowMessageA("TaskbarCreated") / ChangeWindowMessageFilterEx did not work
          Shell_NotifyIconW(NIM_ADD, &g_tray_icon);
        }
        return 0;
      }
    }
    return DefWindowProcW(window, message, wparam, lparam);
  }

  std::filesystem::path get_known_folder_path(REFKNOWNFOLDERID folder_id) {
    auto string = PWSTR{ };
    if (FAILED(SHGetKnownFolderPath(folder_id,
        KF_FLAG_DEFAULT, nullptr, &string)))
      return { };
    auto path = std::filesystem::path(string);
    CoTaskMemFree(string);
    return path;
  }

  std::filesystem::path resolve_config_file_path(std::filesystem::path filename) {
    auto error = std::error_code{ };
    if (filename.empty()) {
      filename = default_config_filename;
      for (auto folder_id : { 
            FOLDERID_Profile, 
            FOLDERID_LocalAppData, 
            FOLDERID_RoamingAppData 
          }) {
        auto path = get_known_folder_path(folder_id) / filename;
        if (std::filesystem::exists(path, error))
          return path;
      }
      // create in profile path when opening for editing
      if (!std::filesystem::exists(filename, error))
        return get_known_folder_path(FOLDERID_Profile) / filename;
    }
    return std::filesystem::absolute(filename, error);
  }
} // namespace

void show_notification(const char* message_) {
  auto& icon = g_tray_icon;
  const auto message = utf8_to_wide(message_);
  icon.uFlags = NIF_INFO;
  lstrcpyW(icon.szInfo, message.c_str());
  Shell_NotifyIconW(NIM_MODIFY, &icon);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
  if (!interpret_commandline(g_settings, __argc, __wargv)) {
    print_help_message();
    return 1;
  }
  g_verbose_output = g_settings.verbose;

  g_settings.config_file_path = 
    resolve_config_file_path(g_settings.config_file_path);

  if (g_settings.check_config) {
    if (!g_config_file.load(g_settings.config_file_path))
      return 1;
    message("The configuration is valid");
    return 0;
  }

  const auto single_instance = LimitSingleInstance(
    "Global\\{0A7DECF3-1D6B-44B3-9596-0584BEC2A0C8}");
  if (single_instance.is_another_instance_running()) {
    error("Another instance is already running");
    return 1;
  }

  const auto window_class_name = L"Keymapper";
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

  if (!connect())
    return 1;

  auto& icon = g_tray_icon;
  icon.cbSize = sizeof(icon);
  icon.hWnd = g_window;
  icon.uID = static_cast<UINT>(
    reinterpret_cast<uintptr_t>(g_window));
  icon.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
  lstrcpyW(icon.szTip, window_class_name);
  icon.uCallbackMessage = WM_APP_TRAY_NOTIFY;
  icon.hIcon = LoadIconW(instance, L"IDI_ICON1");
  Shell_NotifyIconW(NIM_ADD, &icon);

  verbose("Loading configuration file '%ws'", g_settings.config_file_path.c_str());
  g_config_file.load(g_settings.config_file_path);

  send_config();
  
  WTSRegisterSessionNotification(g_window, NOTIFY_FOR_THIS_SESSION);

  auto disable = BOOL{ FALSE };
  SetUserObjectInformationA(GetCurrentProcess(),
    UOI_TIMERPROC_EXCEPTION_SUPPRESSION, &disable, sizeof(disable));
  if (g_settings.auto_update_config)
    SetTimer(g_window, TIMER_UPDATE_CONFIG, update_config_interval_ms, NULL);
  SetTimer(g_window, TIMER_UPDATE_CONTEXT, update_context_inverval_ms, NULL);
  SetTimer(g_window, TIMER_CREATE_TRAY_ICON, recreate_tray_icon_interval_ms, NULL);

  verbose("Entering update loop");
  auto message = MSG{ };
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
  verbose("Exiting");

  Shell_NotifyIconW(NIM_DELETE, &g_tray_icon);

  return 0;
}
