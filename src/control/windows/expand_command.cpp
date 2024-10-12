
#include <string>
#include <sstream>
#include <thread>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace {
  std::wstring multibyte_to_wide(const std::string& str) {
    auto wide = std::wstring();
    wide.resize(MultiByteToWideChar(CP_ACP, 0, 
      str.data(), static_cast<int>(str.size()),
      NULL, 0));
    MultiByteToWideChar(CP_ACP, 0, 
      str.data(), static_cast<int>(str.size()), 
      wide.data(), static_cast<int>(wide.size()));
    return wide;
  }

  std::wstring get_process_output(wchar_t* commandline) {
    auto sattr = SECURITY_ATTRIBUTES{ }; 
    sattr.nLength = sizeof(SECURITY_ATTRIBUTES); 
    sattr.bInheritHandle = TRUE; 
    sattr.lpSecurityDescriptor = NULL; 

    auto stdout_read_handle = HANDLE{ };
    auto stdout_write_handle = HANDLE{ };
    if (!CreatePipe(&stdout_read_handle, &stdout_write_handle, &sattr, 0)) 
      return { };
    if (!SetHandleInformation(stdout_read_handle, HANDLE_FLAG_INHERIT, 0))
      return { };

    auto si = STARTUPINFOW{ };
    si.cb = sizeof(si);
    si.hStdError = stdout_write_handle;
    si.hStdOutput = stdout_write_handle;
    si.dwFlags |= STARTF_USESTDHANDLES;

    auto pi = PROCESS_INFORMATION{ };
    if (CreateProcessW(nullptr, commandline, nullptr, nullptr, 
        true, CREATE_SUSPENDED, nullptr, nullptr, &si, &pi) != TRUE) {
      CloseHandle(stdout_read_handle);
      return { };
    }

    auto ss = std::stringstream();
    auto reader = std::thread{ 
      [&]() noexcept {
        auto bytes_read = DWORD{ };
        char buffer[1024]; 
        for (;;) { 
          const auto success = ReadFile(stdout_read_handle, buffer, 
            sizeof(buffer), &bytes_read, nullptr);
          const auto error = GetLastError();
          if (!success && error != ERROR_MORE_DATA)
            break; 

          ss.write(buffer, bytes_read);
        }
      }
    };

    CloseHandle(stdout_write_handle);  
    ResumeThread(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
  
    reader.join();
    CloseHandle(stdout_read_handle);
  
    return multibyte_to_wide(ss.str());
  }
} // namespace

std::wstring expand_command(std::wstring_view argument) {
  if (argument.find(L"$(") != 0 || argument.back() != L')')
    return std::wstring(argument);

  auto command = std::wstring(L"CMD /C ");
  command += argument.substr(2, argument.size() - 3);
  return get_process_output(const_cast<wchar_t*>(command.c_str()));
}
