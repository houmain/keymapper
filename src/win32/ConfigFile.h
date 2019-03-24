#pragma once

#include "config/Config.h"
#include <ctime>
#include <string>

class ConfigFile {
public:
  ConfigFile() = default;
  explicit ConfigFile(std::wstring filename);
  const Config& config() { return m_config; }
  bool update();

private:
  std::wstring m_filename;
  std::time_t m_modify_time{ };
  Config m_config;
};

std::wstring get_user_directory();
