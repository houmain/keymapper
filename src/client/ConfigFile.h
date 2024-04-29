#pragma once

#include "config/Config.h"
#include "config/StringTyper.h"
#include <ctime>
#include <string>
#include <filesystem>

class ConfigFile {
public:
  bool load(std::filesystem::path filename);
  bool update(bool check_modified = true);
  const Config& config() const { return m_config; }
  const std::filesystem::path& filename() const { return m_filename; }
  explicit operator bool() const { return !m_filename.empty(); }

private:
  std::filesystem::path m_filename;
  std::time_t m_modify_time{ -1 };
  Config m_config;
  StringTyper m_string_typer;
};
