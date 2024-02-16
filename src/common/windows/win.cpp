
#include "win.h"

std::string wide_to_utf8(std::wstring_view str) {
  auto result = std::string();
  result.resize(WideCharToMultiByte(CP_UTF8, 0, 
    str.data(), static_cast<int>(str.size()), 
    NULL, 0, 
    NULL, 0));
  WideCharToMultiByte(CP_UTF8, 0, 
    str.data(), static_cast<int>(str.size()), 
    result.data(), static_cast<int>(result.size()),
    NULL, 0);
  return result;
}

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
