
#include "client/Settings.h"
#include "client/ClientState.h"
#include "common/windows/LimitSingleInstance.h"
#include "common/output.h"
#include "Wtsapi32.h"
#include <WinSock2.h>
#include <shellapi.h>
#include <Shlobj.h>
#include <cctype>

// enable visual styles for message boxes
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace {
  const auto online_help_url = "https://github.com/houmain/keymapper#keymapper";

  const auto WM_APP_RESET = WM_APP + 0;
  const auto WM_APP_SERVER_MESSAGE = WM_APP + 1;
  const auto WM_APP_TRAY_NOTIFY = WM_APP + 2;
  const auto WM_APP_CONTROL_MESSAGE = WM_APP + 3;
  const auto TIMER_UPDATE_CONFIG = 1;
  const auto TIMER_UPDATE_CONTEXT = 2;
  const auto TIMER_CREATE_TRAY_ICON = 3;
  const auto IDI_EXIT = 1;
  const auto IDI_ACTIVE = 2;
  const auto IDI_HELP = 3;
  const auto IDI_OPEN_CONFIG = 4;
  const auto IDI_UPDATE_CONFIG = 5;
  const auto IDI_ABOUT = 6;
  const auto IDI_DEVICES = 7;

  const auto update_context_inverval_ms = 50;
  const auto update_config_interval_ms = 500;
  const auto recreate_tray_icon_interval_ms = 1000;

  Settings g_settings;
  ClientState g_state;
  bool g_was_inaccessible;
  bool g_session_changed;
  HWND g_window;
  NOTIFYICONDATAW g_tray_icon;

  void validate_state() {
    // validate internal state when a window of another user was focused
    // force validation after session change
    const auto check_accessibility = 
      !std::exchange(g_session_changed, false);
    if (check_accessibility) {
      if (g_state.is_focused_window_inaccessible()) {
        g_was_inaccessible = true;
        return;
      }
      if (!std::exchange(g_was_inaccessible, false))
        return;
    }
    verbose("Validating state");
    g_state.send_validate_state();
  }

  bool connect() {
    if (auto socket = g_state.connect_server(); 
        socket && WSAAsyncSelect(*socket, g_window, 
          WM_APP_SERVER_MESSAGE, (FD_READ | FD_CLOSE)) == 0)
      return true;
    error("Connecting to keymapperd failed");
    return false;
  }

  bool listen_for_control() {
    if (auto socket = g_state.listen_for_control_connections();
        socket && WSAAsyncSelect(*socket, g_window,
          WM_APP_CONTROL_MESSAGE, FD_ACCEPT) == 0)
      return true;
    error("Initializing keymapperctl connection failed");
    return false;
  }

  bool accept_control() {
    if (auto socket = g_state.accept_control_connection();
        socket && WSAAsyncSelect(*socket, g_window,
          WM_APP_CONTROL_MESSAGE, (FD_READ | FD_CLOSE)) == 0)
      return true;
    error("Accepting keymapperctl connection failed");
    return false;
  }

  void open_configuration() {
    const auto filename = g_state.config_filename().wstring();

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

  void open_about() {
    const auto caption = "About keymapper";
    auto text = std::vector<char>(1024);
    text.resize(std::snprintf(text.data(), text.size(), 
      "Version %s\n"
      "\n"
      "%s",
      about_header, about_footer));

    MessageBoxA(g_window, text.data(), caption, MB_ICONINFORMATION | MB_TOPMOST);
  }

  void open_tray_menu() {
    auto popup_menu = CreatePopupMenu();
    AppendMenuW(popup_menu, 
      (g_state.is_active() ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, IDI_ACTIVE, L"Active");
    AppendMenuW(popup_menu, MF_STRING, IDI_OPEN_CONFIG, L"Configuration");
    if (!g_settings.auto_update_config)
      AppendMenuW(popup_menu, MF_STRING, IDI_UPDATE_CONFIG, L"Reload");
    AppendMenuW(popup_menu, MF_STRING, IDI_DEVICES, L"Devices");
    AppendMenuW(popup_menu, MF_STRING, IDI_HELP, L"Help");
    AppendMenuW(popup_menu, MF_STRING, IDI_ABOUT, L"About");
    AppendMenuW(popup_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(popup_menu, MF_STRING, IDI_EXIT, L"Exit");
    SetForegroundWindow(g_window);
    auto cursor_pos = POINT{ };
    GetCursorPos(&cursor_pos);
    TrackPopupMenu(popup_menu, 
      TPM_NOANIMATION | TPM_VCENTERALIGN,
      cursor_pos.x + 7, cursor_pos.y, 0, g_window, nullptr);
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
          g_state.toggle_active();
        else if (lparam == 0x405)
          open_configuration();
        return 0;

      case WM_APP_SERVER_MESSAGE:
        if (lparam == FD_READ) {
          g_state.read_server_messages();
        }
        else {
          verbose("Connection to keymapperd lost");
          verbose("---------------");
          if (!connect() || !g_state.send_config())
            PostQuitMessage(1);
        }
        return 0;

      case WM_APP_CONTROL_MESSAGE:
        if (lparam == FD_ACCEPT) {
          accept_control();
        }
        else {
          g_state.read_control_messages();
        }
        return 0;

      case WM_COMMAND:
        switch (wparam) {
          case IDI_ACTIVE:
            g_state.toggle_active();
            return 0;

          case IDI_OPEN_CONFIG:
            open_configuration();
            return 0;

          case IDI_UPDATE_CONFIG:
            if (g_state.update_config(false))
              g_state.send_config();
            return 0;

          case IDI_DEVICES:
            g_state.request_device_names();
            return 0;

          case IDI_HELP:
            open_online_help();
            return 0;

          case IDI_ABOUT:
            open_about();
            return 0;

          case IDI_EXIT:
            PostQuitMessage(0);
            return 0;
        }
        break;

      case WM_TIMER: {
        if (wparam == TIMER_UPDATE_CONTEXT) {
          if (g_state.update_active_contexts())
            g_state.send_active_contexts();
          validate_state();
        }
        else if (wparam == TIMER_UPDATE_CONFIG) {
          if (g_state.update_config(true))
            g_state.send_config();
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
  
  void show_notification(const char* message_) {
    auto& icon = g_tray_icon;
    const auto message = utf8_to_wide(message_);
    icon.uFlags = NIF_INFO;
    lstrcpyW(icon.szInfo, message.c_str());
    Shell_NotifyIconW(NIM_MODIFY, &icon);
  }
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
  if (!interpret_commandline(g_settings, __argc, __wargv)) {
    print_help_message();
    return 1;
  }
  g_verbose_output = g_settings.verbose;
  if (!g_settings.no_notify)
    g_show_notification = &show_notification;

  g_settings.config_file_path = 
    resolve_config_file_path(g_settings.config_file_path);

  if (g_settings.check_config) {
    if (!g_state.load_config(g_settings.config_file_path))
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

  SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

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

  listen_for_control();

  if (!g_settings.no_tray_icon) {
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

    SetTimer(g_window, TIMER_CREATE_TRAY_ICON, recreate_tray_icon_interval_ms, NULL);
  }

  verbose("Loading configuration file '%ws'", g_settings.config_file_path.c_str());
  if (!g_state.load_config(g_settings.config_file_path)) {
    // exit when there is no configuration and user cannot update it
    if (g_settings.no_tray_icon)
      return 1;
  }
  g_state.send_config();
  
  WTSRegisterSessionNotification(g_window, NOTIFY_FOR_THIS_SESSION);

  auto disable = BOOL{ FALSE };
  SetUserObjectInformationA(GetCurrentProcess(),
    UOI_TIMERPROC_EXCEPTION_SUPPRESSION, &disable, sizeof(disable));
  if (g_settings.auto_update_config)
    SetTimer(g_window, TIMER_UPDATE_CONFIG, update_config_interval_ms, NULL);
  SetTimer(g_window, TIMER_UPDATE_CONTEXT, update_context_inverval_ms, NULL);

  auto message = MSG{ };
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
  verbose("Exiting");

  if (!g_settings.no_tray_icon)
    Shell_NotifyIconW(NIM_DELETE, &g_tray_icon);

  return 0;
}
