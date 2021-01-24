
#include "ConfigFile.h"
#include "config/ParseConfig.h"
#include "output.h"
#include <cstdio>
#include <fstream>
#include <unistd.h>
#include <pwd.h>
#include <sys/stat.h>

std::time_t get_modify_time(const std::string& filename) {
  using stat_t = struct stat;
  auto st = stat_t{ };
  if (::stat(filename.c_str(), &st) == 0)
    return st.st_mtime;
  return { };
}

std::string get_home_directory() {
  if (auto homedir = ::getenv("HOME"))
    return homedir;
  return ::getpwuid(::getuid())->pw_dir;
}

ConfigFile::ConfigFile(std::string filename)
  : m_filename(std::move(filename)) {
}

bool ConfigFile::update() {
  const auto modify_time = get_modify_time(m_filename);
  if (modify_time == m_modify_time)
    return false;
  m_modify_time = modify_time;

  auto is = std::ifstream(m_filename);
  if (is.good()) {
    try {
      auto parse = ParseConfig();
      m_config = parse(is);
    }
    catch (const std::exception& ex) {
      error("%s", ex.what());
      return false;
    }
  }
  return true;
}
