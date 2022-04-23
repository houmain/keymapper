#pragma once

#include "win.h"

// source: http://support.microsoft.com/kb/243953
class LimitSingleInstance {
public:
  LimitSingleInstance(const char* strMutexName) {
    m_hMutex = CreateMutexA(NULL, FALSE, strMutexName);
    m_dwLastError = GetLastError();
  }
  ~LimitSingleInstance()  {
    if (m_hMutex)
      CloseHandle(m_hMutex);
  }
  bool is_another_instance_running() const {
    return (ERROR_ALREADY_EXISTS == m_dwLastError);
  }

private:
  DWORD m_dwLastError;
  HANDLE m_hMutex;
};
