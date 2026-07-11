
#include "common/windows/win.h"
#include <string>

namespace {
  bool file_exists(const std::wstring& filename) {
    return (GetFileAttributesW(filename.c_str()) != INVALID_FILE_ATTRIBUTES);
  }

  bool file_exists_complete_extension(std::wstring& filename) {
    if (file_exists(filename))
      return true;

    filename += L".exe";
    const auto exists = file_exists(filename);
    filename.resize(filename.size() - 4);
    return exists;
  }

  bool contains_terminal_control_characters(std::wstring_view args) {
    auto in_string = false;
    for (auto c : args) {
      if (c == '"')
        in_string = !in_string;

      if (!in_string && std::strchr("&|>%$", c))
        return true;
    }
    return false;
  }

  bool is_identifier(wchar_t c) {
    return (std::isalnum(static_cast<unsigned char>(c)) || c == '_');
  }

  void remove_last_token(std::wstring& string) {
    if (!string.empty()) {
      const auto skipping = is_identifier(string.back());
      string.pop_back();
      while (!string.empty() && is_identifier(string.back()) == skipping)
        string.pop_back();
    }
  }

  std::wstring get_initial_string(std::wstring_view command) {
    if (!command.empty() &&
        (command.front() == '\'' || command.front() == '\"')) {
      const auto string_end = command.find(command.front(), 1);
      if (string_end != std::string::npos)
        return std::wstring(command.substr(1, string_end - 1));
    }
    return { };
  }

  std::wstring get_first_word(std::wstring_view command) {
    auto it = command.begin();
    while (it != command.end() && is_identifier(*it))
      ++it;
    return std::wstring(command.data(), std::distance(command.begin(), it));
  }
  
  std::wstring get_filename(std::wstring_view command) {
    auto string = get_initial_string(command);
    if (file_exists_complete_extension(string))
      return string;

    string = get_first_word(command);
    if (file_exists_complete_extension(string))
      return string;

    string = std::wstring(command);
    while (!string.empty()) {
      if (file_exists_complete_extension(string))
        return string;
      remove_last_token(string);
    }
    return { };
  }

  bool create_process(wchar_t* command_line) {
    auto flags = DWORD{ CREATE_NO_WINDOW };
    auto startup_info = STARTUPINFOW{ sizeof(STARTUPINFOW) };
    auto process_info = PROCESS_INFORMATION{ };
    if (!CreateProcessW(nullptr, command_line, nullptr, nullptr, FALSE, 
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
    return create_process(command.data());
  }
  else {
    command.insert(0, L"CMD /C ");
    return create_process(command.data());
  }
}
