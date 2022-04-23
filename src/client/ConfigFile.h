#pragma once

#include "config/Config.h"
#include <ctime>
#include <string>
#include <filesystem>

class ConfigFile {
public:
  explicit ConfigFile(std::filesystem::path filename);

  bool update();
  const Config& config() const { return m_config; }

private:
  const std::filesystem::path m_filename;
  std::time_t m_modify_time{ -1 };
  Config m_config;
};
