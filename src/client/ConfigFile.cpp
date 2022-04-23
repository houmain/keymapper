
#include "ConfigFile.h"
#include "config/ParseConfig.h"
#include "common/common.h"
#include <cstdio>
#include <fstream>

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
      return { -1 };
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

ConfigFile::ConfigFile(std::filesystem::path filename)
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
