
#include "common/windows/win.h"
#include <string>

namespace {
  std::wstring get_terminal_filename() {
    auto cmd = std::wstring(MAX_PATH, ' ');
    cmd.resize(GetSystemDirectoryW(cmd.data(), static_cast<UINT>(cmd.size())));
    cmd += L"\\CMD.EXE";
    return cmd;
  }

  bool file_exists(const std::wstring& filename) {
    return (GetFileAttributesW(filename.c_str()) != INVALID_FILE_ATTRIBUTES);
  }

  bool contains_terminal_control_characters(std::wstring_view args) {
    auto in_string = false;
    for (auto c : args) {
      if (c == '"')
        in_string = !in_string;

      if (!in_string && std::strchr("&|>%", c))
        return true;
    }
    return false;
  }
  
  std::wstring_view get_filename(std::wstring_view command) {
    if (!command.empty() && 
        (command.front() == '\'' || command.front() == '\"')) {
      const auto string_end = command.find(command.front(), 1);
      if (string_end != std::string::npos &&
          file_exists(std::wstring(command.substr(1, string_end - 1))))
        return command.substr(1, string_end - 1);
      return { };
    }

    auto string = std::wstring(command);
    while (!string.empty()) {
      if (file_exists(string))
        return command.substr(0, string.size());
      string.pop_back();
    }
    return { };
  }

  bool create_process(const wchar_t* filename, wchar_t* command_line) {
    auto flags = DWORD{ CREATE_NO_WINDOW };
    auto startup_info = STARTUPINFOW{ sizeof(STARTUPINFOW) };
    auto process_info = PROCESS_INFORMATION{ };
    if (!CreateProcessW(filename, command_line, nullptr, nullptr, FALSE, 
        flags, nullptr, nullptr, &startup_info, &process_info)) 
      return false;
    
    CloseHandle(process_info.hProcess);
    CloseHandle(process_info.hThread);
    return true;
  }
} // namespace

bool execute_terminal_command(const std::string& command_utf8) {
  auto command = utf8_to_wide(command_utf8);
  const auto filename = get_filename(command);
  if (!filename.empty() && 
      !contains_terminal_control_characters(command.substr(filename.size()))) {
    return create_process(nullptr, command.data());
  }
  else {
    static const auto cmd = get_terminal_filename();
    command.insert(0, L"/C ");
    return create_process(cmd.c_str(), command.data());
  }
}
