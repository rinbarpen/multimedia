#pragma once

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include <multimedia/common/Platform.hpp>
#include <multimedia/common/StringUtil.hpp>

#if defined(__LINUX__)
# include <dirent.h>
# include <fcntl.h>
# include <signal.h>
# include <sys/stat.h>
# include <sys/types.h>
# include <unistd.h>
#elif defined(__WIN__)
# include <direct.h>
# include <io.h>
# include <fcntl.h>
# include <sys/stat.h>
# include <windows.h>
#endif

namespace os_api
{
namespace detail
{
static int lstat(const char *file, struct stat *st = nullptr) {
  struct stat lst;
  int r{-1};
#if defined(__LINUX__) 
  r = ::lstat(file, &lst);
#elif defined(__WIN__)
  r = ::stat(file, &lst);
#endif
  if (st) *st = lst;
  return r;
}
static bool touch(
  const std::string &filename, int oflag = 0644) {
  int fd{-1};
#if defined(__LINUX__)
  if ((fd = ::open(filename.c_str(), O_CREAT | O_WRONLY, oflag)) == 0) {
    ::close(fd);
    return true;
  }
#elif defined(__WIN__)
  if ((fd = ::_open(filename.c_str(), O_CREAT | O_WRONLY, oflag)) == 0) {
    ::_close(fd);
    return true;
  }
#endif
  return false;
}
static int mkdir(const std::string &dirname) {
#if defined(__LINUX__)
  if (::access(dirname.c_str(), F_OK) == 0) {
    return 0;
  }
  return ::mkdir(dirname.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#elif defined(__WIN__)
  if (::_access(dirname.c_str(), 0) == 0) {
    return 0;
  }
  return ::_mkdir(dirname.c_str());
#endif
  return -1;
}
static void list_all_file(std::vector<std::string> &files,
  const std::string &path, const std::string &suffix) {
#if defined(__LINUX__)
  if (::access(path.c_str(), 0) != 0) return;

  DIR *dir = ::opendir(path.c_str());
  if (dir == nullptr) return;

  struct dirent *dp = nullptr;
  while ((dp = ::readdir(dir)) != nullptr) {
    if (dp->d_type == DT_DIR) {
      if (!std::strcmp(dp->d_name, ".") || !std::strcmp(dp->d_name, "..")) {
        continue;
      }
      detail::list_all_file(files, path + "/" + dp->d_name, suffix);
    }
    else if (dp->d_type == DT_REG) {
      std::string filename(dp->d_name);
      if (suffix.empty()) {
        files.push_back(path + "/" + filename);
        continue;
      }
      if (filename.size() < suffix.size()) {
        continue;
      }
      if (filename.substr(filename.length() - suffix.size()) == suffix) {
        files.push_back(path + "/" + filename);
      }
    }
  }
  ::closedir(dir);
#elif defined(__WIN__)
  struct _finddata_t file_info;
  std::string search_path = path + "/*";
  intptr_t handle = _findfirst(search_path.c_str(), &file_info);
  if (handle != -1) {
    do {
      if (file_info.attrib & _A_SUBDIR) {
        // Skip "." and ".." directories
        if (strcmp(file_info.name, ".") != 0
            && strcmp(file_info.name, "..") != 0) {
          list_all_file(files, path + "/" + file_info.name, suffix);
        }
      }
      else {
        std::string filename(file_info.name);
        if (suffix.empty()
            || filename.size() >= suffix.size()
            && filename.compare(filename.size() - suffix.size(), suffix.size(), suffix)
                      == 0) {
          files.push_back(path + "/" + filename);
        }
      }
    } while (_findnext(handle, &file_info) == 0);
    _findclose(handle);
  }
#endif
}
}  // namespace detail

static bool exist(const std::string &path, bool is_dir) {
#if defined(__LINUX__)
  struct stat st;
  if (detail::lstat(path.c_str(), &st) == 0) {
    if (is_dir && !S_ISDIR(st.st_mode)) {
      return false;
    }
    return true;
  }
#elif defined(__WIN__)
  struct stat st;
  if (detail::lstat(path.c_str(), &st) == 0) {
    if (is_dir && !(st.st_mode & _S_IFDIR)) {
      return false;
    }
    return true;
  }
#endif
  return false;
}
static bool exist_file(const std::string &path) { return exist(path, false); }
static bool exist_dir(const std::string &path) {
  return exist(path, true);
}

static bool touch(const std::string &filename, int oflag = 0644) {
  if (exist_file(filename.c_str())) {
    return false;
  }

  char *path = ::strdup(filename.c_str());
  char *ptr = ::strchr(path + 1, '/');
  for (; ptr; *ptr = '/', ptr = strchr(ptr + 1, '/')) {
    *ptr = '\0';
    if (detail::mkdir(path) != 0) {
      ::free(path);
      return false;
    }
  }
  if (ptr != nullptr) {
    ::free(path);
    return false;
  }
  else if (detail::touch(filename, oflag)) {
    ::free(path);
    return false;
  }
  ::free(path);
  return true;
}
static bool mkdir(const std::string &dirname) {
  if (exist_dir(dirname.c_str()) == 0) {
    return true;
  }

  char *path = ::strdup(dirname.c_str());
  char *ptr = ::strchr(path + 1, '/');
  for (; ptr; *ptr = '/', ptr = strchr(ptr + 1, '/')) {
    *ptr = '\0';
    if (detail::mkdir(path) != 0) {
      ::free(path);
      return false;
    }
  }
  if (ptr != nullptr) {
    ::free(path);
    return false;
  }
  else if (detail::mkdir(path) != 0) {
    ::free(path);
    return false;
  }
  ::free(path);
  return true;
}
static bool mk(const std::string &path, bool is_dir) {
  if (is_dir) {
    return mkdir(path);
  }

  return touch(path);
}

static std::vector<std::string> list_all_file(
  const std::string &path, const std::string &suffix = "") {
  std::vector<std::string> files;
#if defined(__LINUX__)
  if (::access(path.data(), 0) != 0) return files;

  DIR *dir = ::opendir(path.data());
  if (dir == nullptr) return files;

  struct dirent *dp = nullptr;
  while ((dp = ::readdir(dir)) != nullptr) {
    if (dp->d_type == DT_DIR) {
      if (!std::strcmp(dp->d_name, ".") || !std::strcmp(dp->d_name, "..")) {
        continue;
      }
      detail::list_all_file(files, path + "/" + std::string(dp->d_name), suffix);
    }
    else if (dp->d_type == DT_REG) {
      std::string filename(dp->d_name);
      if (suffix.empty()) {
        files.push_back(path + "/" + filename);
        continue;
      }
      if (filename.size() < suffix.size()) {
        continue;
      }
      if (filename.substr(filename.length() - suffix.size()) == suffix) {
        files.push_back(path + "/" + filename);
      }
    }
  }
  ::closedir(dir);
#elif defined(__WIN__)
  struct _finddata_t file_info;
  std::string search_path = path + "/*";
  intptr_t handle = _findfirst(search_path.c_str(), &file_info);
  if (handle != -1) {
    do {
      if (file_info.attrib & _A_SUBDIR) {
        // Skip "." and ".." directories
        if (strcmp(file_info.name, ".") != 0
            && strcmp(file_info.name, "..") != 0) {
          detail::list_all_file(files, path + "/" + std::string(file_info.name), suffix);
        }
      }
      else {
        std::string filename(file_info.name);
        if (suffix.empty()
            || filename.size() >= suffix.size()
                 && filename.compare(
                      filename.size() - suffix.size(), suffix.size(), suffix)
                      == 0) {
          files.push_back(path + "/" + filename);
        }
      }
    } while (_findnext(handle, &file_info) == 0);
    _findclose(handle);
  }
#endif
  return files;
}
static bool is_executing_file(const std::string &pidfile) {
  if (!exist_file(pidfile)) {
    return false;
  }

  std::ifstream ifs(pidfile);
  std::string line;
  if (!ifs || !std::getline(ifs, line)) {
    return false;
  }
  if (line.empty()) {
    return false;
  }
#if defined(__LINUX__)
  pid_t pid = atoi(line.c_str());
  if (pid <= 1) {
    return false;
  }

  if (::kill(pid, 0) != 0) {
    return false;
  }
#elif defined(__WIN__)
  DWORD pid = atoi(line.c_str());
  HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
  if (processHandle != NULL) {
    CloseHandle(processHandle);
    return true;
  }
#endif
  return true;
}
static bool unlink(const std::string &filename) {
  if (exist_file(filename)) return true;

  return ::unlink(filename.c_str()) == 0;
}
static bool rm(const std::string &path) {
  struct stat st;
  if (detail::lstat(path.c_str(), &st) != 0) {
    return true;
  }
  // symlink file
  if (!(st.st_mode & S_IFDIR)) {
    return unlink(path);
  }

#if defined(__LINUX__)
  DIR *dir = ::opendir(path.c_str());
  if (nullptr == dir) {
    return false;
  }

  bool r = false;
  struct dirent *dp = nullptr;
  while ((dp = readdir(dir)) != nullptr) {
    if (!std::strcmp(dp->d_name, ".") || !std::strcmp(dp->d_name, "..")) {
      continue;
    }
    std::string dirname = path + "/" + dp->d_name;
    r = rm(dirname);
  }
  ::closedir(dir);

  if (0 == ::rmdir(path.c_str())) {
    r = true;
  }
  return r;
#elif defined(__WIN__)
  DWORD attributes = GetFileAttributes(path.c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES) {
    return false;
  }

  if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
    WIN32_FIND_DATA findData;
    HANDLE hFind = FindFirstFile((path + "/*").c_str(), &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
      do {
        if (strcmp(findData.cFileName, ".") != 0
            && strcmp(findData.cFileName, "..") != 0) {
          std::string subPath = path + "/" + findData.cFileName;
          rm(subPath);
        }
      } while (FindNextFile(hFind, &findData));
      FindClose(hFind);
    }
  }

  return RemoveDirectory(path.c_str()) || DeleteFile(path.c_str());
#endif
}
/**
 * @brief Firstly, remove that file with the same path if the file exists.
 *        Then, create a system link with the path
 *
 * @param from
 * @param to
 * @return bool
 *
 */
static bool move(const std::string &from, const std::string &to) {
  if (!rm(to)) {
    return false;
  }
  return 0 == ::rename(from.c_str(), to.c_str());
}
static bool realpath(const std::string &path, std::string &rpath) {
  if (0 != detail::lstat(path.c_str())) {
    return false;
  }
#if defined(__LINUX__)
  char *p = ::realpath(path.c_str(), nullptr);
  if (nullptr == p) {
    return false;
  }

  std::string(p).swap(rpath);
  ::free(p);
#elif defined(__WIN__)
  DWORD length = GetFullPathName(path.c_str(), 0, nullptr, nullptr);
  if (length == 0) {
    return false;
  }

  char *buffer = (char*)::malloc(length);
  if (buffer == nullptr) {
    return false;
  }

  if (GetFullPathName(path.c_str(), length, buffer, nullptr) == 0) {
    ::free(buffer);
    return false;
  }

  rpath.assign(buffer);
  ::free(buffer);
#endif

  return true;
}
/**
 * @brief Firstly, remove that file with the same path if the file exists.
 *        Then, create a system link with the path
 *
 * @param from
 * @param to
 * @return bool
 *
 */
static bool symlink(const std::string &from, const std::string &to) {
  if (!rm(to)) {
    return false;
  }
#if defined(__LINUX__)
  return 0 == ::symlink(from.c_str(), to.c_str());
#elif defined(__WIN__)
  return CreateSymbolicLink(
           to.c_str(), from.c_str(), SYMBOLIC_LINK_FLAG_DIRECTORY)
         != 0;
#endif
}
/**
 * @brief Get the name of the directory in the file.
 *        Return the current directory if file doesn't exist
 *
 * @param filename
 * @return std::string
 * 
 */
static std::string dirname(const std::string &filename) {
  if (filename.empty()) {
    return ".";
  }

  auto pos = filename.rfind('/');
  if (pos == 0) {
    return "/";
  }
  else if (pos == std::string::npos) {
    return ".";
  }
  return filename.substr(0, pos);
}

/**
 * @brief get the basename of the directory in the file
 * @param filename 
 * @return std::string
 * 
 */
static std::string basename(const std::string &filename) {
  if (filename.empty()) {
    return filename;
  }

  auto pos = filename.rfind('/');
  if (pos == std::string::npos) {
    return filename;
  }
  return filename.substr(pos + 1);
}

}  // namespace os_api
