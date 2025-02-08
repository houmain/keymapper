
#include "ConfigFile.h"
#include "config/ParseConfig.h"
#include "common/output.h"
#include <cstdio>
#include <fstream>
#include <thread>

#if defined(_WIN32)

#include "common/windows/win.h"

namespace {
  std::time_t filetime_to_time_t(const FILETIME& ft) {
    auto ull = ULARGE_INTEGER{ };
    ull.LowPart = ft.dwLowDateTime;
    ull.HighPart = ft.dwHighDateTime;
    return ull.QuadPart / 10000000ULL - 11644473600ULL;
  }

  std::time_t get_modify_time(const std::filesystem::path& filename) {
    auto file_attr_data = WIN32_FILE_ATTRIBUTE_DATA{ };
    if (!GetFileAttributesExW(filename.c_str(),
        GetFileExInfoStandard, &file_attr_data))
      return { };
    return filetime_to_time_t(file_attr_data.ftLastWriteTime);
  }
} // namespace

#else // !defined(_WIN32)

#include <sys/stat.h>

namespace {
  std::time_t get_modify_time(const std::string& filename) {
    using stat_t = struct stat;
    auto st = stat_t{ };
    if (::stat(filename.c_str(), &st) == 0)
      return st.st_mtime;
    return { };
  }
} // namespace

#endif // !defined(_WIN32)

namespace {
  std::time_t get_latest_modify_time(const std::filesystem::path& filename, 
      const std::vector<std::filesystem::path>& include_filenames) {
    auto time = get_modify_time(filename);
    if (time)
      for (const auto& include_filename : include_filenames)
        if (const auto include_time = get_modify_time(include_filename))
          time = std::max(time, include_time);
    return time;
  }
} // namespace

bool ConfigFile::load(std::filesystem::path filename) {
  m_filename = std::move(filename);
  m_modify_time = { -1 };
  return update();
}

bool ConfigFile::update(bool check_modified) {
  const auto modify_time = get_latest_modify_time(
    m_filename, m_config.include_filenames);
  if (check_modified && 
      modify_time == m_modify_time)
    return false;
  m_modify_time = modify_time;
  try {
    // do not reload too quickly after a modification was detected
    // at least saving with gedit resulted in reading and empty configuration
    if (check_modified && !m_config.contexts.empty())
      std::this_thread::sleep_for(std::chrono::milliseconds(250));
  
    auto is = std::ifstream(m_filename);
    if (is.good()) {
      auto parse = ParseConfig();
      m_config = parse(is, m_filename.parent_path());
      return true;
    }
    else {
      error("Opening configuration file failed");
    }
  }
  catch (const std::exception& ex) {
    error("%s", ex.what());
  }
  return false;
}
