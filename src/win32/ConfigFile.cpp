
#include "ConfigFile.h"
#include "config/ParseConfig.h"
#include "common.h"
#include <shlobj.h>
#include <array>
#include <fstream>

static std::time_t filetime_to_time_t(const FILETIME& ft) {
  auto ull = ULARGE_INTEGER{ };
  ull.LowPart = ft.dwLowDateTime;
  ull.HighPart = ft.dwHighDateTime;
  return ull.QuadPart / 10000000ULL - 11644473600ULL;
}

static std::time_t get_modify_time(const std::wstring& filename) {
  auto file_attr_data = WIN32_FILE_ATTRIBUTE_DATA{ };
  if (!GetFileAttributesExW(filename.c_str(),
      GetFileExInfoStandard, &file_attr_data))
    return { };
  return filetime_to_time_t(file_attr_data.ftLastWriteTime);
}

std::wstring get_user_directory() {
  auto path = std::array<WCHAR, MAX_PATH>();
  SHGetFolderPathW(NULL, CSIDL_PROFILE | CSIDL_FLAG_CREATE,
    NULL, SHGFP_TYPE_CURRENT, path.data());
  return path.data();
}

ConfigFile::ConfigFile(std::wstring filename)
  : m_filename(filename) {
}

bool ConfigFile::update() {
  const auto modify_time = get_modify_time(m_filename);
  if (modify_time == m_modify_time)
    return false;
  m_modify_time = modify_time;

#if defined(_MSC_VER)
  auto is = std::ifstream(m_filename);
#else
  auto is = std::ifstream(std::string(cbegin(m_filename), cend(m_filename)));
#endif
  if (is.good()) {
    try {
      ParseConfig parse;
      m_config = parse(is);
      return true;
    }
    catch (const std::exception& ex) {
      print((std::string("parsing configuration failed:\n") + ex.what() + ".\n").c_str());
    }
  }
  return false;
}
