#pragma once

#include "config/Config.h"
#include <ctime>
#include <string>

class ConfigFile {
public:
  explicit ConfigFile(std::string filename);

  bool update();
  const Config& config() const { return m_config; }

private:
  const std::string m_filename;
  std::time_t m_modify_time{ };
  Config m_config;
};

std::string get_home_directory();
