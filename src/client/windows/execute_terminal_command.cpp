
#include "common/windows/win.h"
#include <string>

namespace {
  std::wstring get_terminal_filename() {
    auto cmd = std::wstring(MAX_PATH, ' ');
    cmd.resize(GetSystemDirectoryW(cmd.data(), static_cast<UINT>(cmd.size())));
    cmd += L"\\CMD.EXE";
    return cmd;
  }

  bool file_exists(const wchar_t* filename) {
    return (GetFileAttributesW(filename) != INVALID_FILE_ATTRIBUTES);
  }

  bool contains_terminal_control_characters(const wchar_t* args) {
    auto in_string = false;
    for (auto it = args; *it; ++it) {
      const auto c = *it;      
      if (c == '"')
        in_string = !in_string;

      if (!in_string && std::strchr("&|>%", c))
        return true;
    }
    return false;
  }
  
  std::pair<wchar_t*, wchar_t*> split_filename_and_args(wchar_t* command) {
    for (auto it = command; ; ++it) {
      const auto c = *it;
      if (c == ' ' || c == '\0') {
        // null terminate potential filename
        *it = '\0';
        if (file_exists(command)) {
          const auto args = (c == '\0' ? it : it + 1);
          if (!contains_terminal_control_characters(args))
            return { command, args };
        }
        if (c == '\0')
          return { };
        // restore space
        *it = c;
      }
    }
  }

  bool create_process(const wchar_t* filename, wchar_t* args) {
    auto flags = DWORD{ CREATE_NO_WINDOW };
    auto startup_info = STARTUPINFOW{ sizeof(STARTUPINFOW) };
    auto process_info = PROCESS_INFORMATION{ };
    if (!CreateProcessW(filename, args, nullptr, nullptr, FALSE, 
        flags, nullptr, nullptr, &startup_info, &process_info)) 
      return false;
    
    CloseHandle(process_info.hProcess);
    CloseHandle(process_info.hThread);
    return true;
  }

  // https://www.codeproject.com/Tips/76427/How-to-bring-window-to-top-with-SetForegroundWindo
  void SetForegroundWindowInternal(HWND hWnd) {
    // Press the "Alt" key
    auto ip = INPUT{ };
    ip.type = INPUT_KEYBOARD;
    ip.ki.wVk = VK_MENU;
    SendInput(1, &ip, sizeof(INPUT));

    ::Sleep(100);
    ::SetForegroundWindow(hWnd);

    // Release the "Alt" key
    ip.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &ip, sizeof(INPUT));
  }
} // namespace

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

bool execute_terminal_command(HWND hwnd, std::string_view command_utf8) {
  auto command = utf8_to_wide(command_utf8);
  auto [filename, arguments] = split_filename_and_args(command.data());
  if (filename) {
    SetForegroundWindowInternal(hwnd);
    return create_process(filename, arguments);
  }
  else {
    static const auto cmd = get_terminal_filename();
    command.insert(0, L"/C ");
    return create_process(cmd.c_str(), command.data());
  }
  return true;
}
