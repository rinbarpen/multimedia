#pragma once

#include <cstdlib>
#include <cstring>
#include <clocale>
#include <algorithm>
#include <initializer_list>
#include <array>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <string_view>
#include <type_traits>
#include <optional>
#include <utility>

#include <multimedia/common/Traits.hpp>

namespace string_util
{
static auto split(::std::string_view s, ::std::string_view delim) noexcept(true)
  -> std::vector<std::string> {
  std::vector<std::string> result;
  size_t start = 0;
  size_t end = 0;

  while ((end = s.find(delim, start)) != std::string::npos) {
    result.emplace_back(s.substr(start, end - start));
    start = end + delim.length();
  }

  result.emplace_back(s.substr(start));
  return result;
}
static auto start_with(::std::string_view s, ::std::string_view match) noexcept(true)
  -> bool {
  size_t len = match.length();
  if (len > s.length()) {
    return false;
  }

  for (size_t i = 0; i < len; ++i) {
    if (s[i] != match[i]) return false;
  }
  return true;
}
static auto end_with(::std::string_view s, ::std::string_view match) noexcept(true)
  -> bool {
  size_t len = match.length();
  size_t slen = s.length();
  if (len > slen) {
    return false;
  }

  for (size_t i = 0; i < len; ++i) {
    if (s[slen - len + i] != match[i]) {
      return false;
    }
  }
  return true;
}

static auto ltrim(::std::string_view s,
  ::std::string_view delim = " \n\t\r\v\f") noexcept(true) -> std::string {
  size_t begin = s.find_first_not_of(delim);
  if (begin == std::string::npos) return "";
  return std::string(s.substr(begin));
}
static auto rtrim(::std::string_view s,
  ::std::string_view delim = " \n\t\r\v\f") noexcept(true) -> std::string {
  size_t end = s.find_last_not_of(delim);
  if (end == std::string::npos) return "";
  return std::string(s.substr(0, end));
}
static auto trim(::std::string_view s, ::std::string_view delim = " \n\t\r\v\f") noexcept(true)
  -> std::string {
  size_t begin = s.find_first_not_of(delim);
  if (begin == std::string::npos) return "";
  size_t end = s.find_last_not_of(delim);
  return std::string(s.substr(begin, end - begin + 1));
}
static auto ltrim(std::string &s, ::std::string_view delim = " \n\t\r\v\f") noexcept(true)
  -> std::string & {
  size_t begin = s.find_first_not_of(delim);
  if (begin == std::string::npos) {
    s = "";
  }
  else {
    s = s.substr(begin);
  }
  return s;
}
static auto rtrim(std::string &s, ::std::string_view delim = " \n\t\r\v\f") noexcept(true)
  -> std::string & {
  size_t end = s.find_last_not_of(delim);
  if (end == std::string::npos) {
    s = "";
  }
  else {
    s = s.substr(0, end);
  }
  return s;
}
static auto trim(std::string &s, ::std::string_view delim = " \n\t\r\v\f") noexcept(true)
  -> std::string & {
  size_t begin = s.find_first_not_of(delim);
  if (begin == std::string::npos) {
    s = "";
  }
  else {
    size_t end = s.find_last_not_of(delim);
    s = s.substr(begin, end - begin + 1);
  }
  return s;
}

static auto equal(::std::string_view s1, ::std::string_view s2,
  bool upper_lower_sensitive = false) noexcept(true) -> bool {
  if (upper_lower_sensitive) {
    return 0 == s1.compare(s2);
  }

  if (s1.length() != s2.length()) {
    return false;
  }

  const char *p1 = s1.data(), *p2 = s2.data();
  for (; *p1 && *p2; ++p1, ++p2) {
    if (*p1 != *p2) {
      return false;
    }
  }
  if (*p1 || *p2) {
    return false;
  }

  return true;
}

static auto to_upper(::std::string_view s) noexcept(true) -> std::string {
  std::string r;
  r.reserve(s.length());
  for (auto c : s) {
    r.push_back(::std::toupper(c));
  }
  return r;
}
static auto to_lower(::std::string_view s) noexcept(true) -> std::string {
  std::string r;
  r.reserve(s.length());
  for (auto c : s) {
    r.push_back(::std::tolower(c));
  }
  return r;
}
static auto to_upper(std::string &s) noexcept(true) -> std::string & {
  for (char &it : s) {
    it = ::std::toupper(it);
  }
  return s;
}
static auto to_lower(std::string &s) noexcept(true) -> std::string & {
  for (char &it : s) {
    it = ::std::tolower(it);
  }
  return s;
}

static auto split(
  ::std::string_view s, std::initializer_list<::std::string_view> delimList) noexcept(true)
  -> std::vector<std::string> {
  std::vector<std::string> result;
  size_t start = 0;
  size_t end = 0;

  while (true) {
    size_t expectedEnd = std::string::npos;
    std::string_view matched;

    for (auto &delim : delimList) {
      expectedEnd = s.find(delim, start);
      if (expectedEnd != std::string::npos) {
        if (expectedEnd < end) end = expectedEnd;
        matched = delim;
      }
    }
    // no matched
    if (end == std::string::npos) {
      result.emplace_back(s.substr(start));
      return result;
    }
    start = end + matched.length();
  }
}

/* net */
static auto parse_url_params(::std::string_view url) noexcept(true)
  -> std::unordered_map<std::string, std::string> {
  std::unordered_map<std::string, std::string> ret;

  auto &&paramString = url.substr(url.find('?') + 1);
  auto &&params = string_util::split(paramString, "&");
  for (const auto &param : params) {
    size_t split_index = param.find('=');
    ret.emplace(param.substr(0, split_index), param.substr(split_index + 1));
  }

  return ret;
}
static auto split_httplike_packet(::std::string_view str) noexcept(true)
  -> std::tuple<std::string, std::string, std::string> {
  size_t curr = 0, last = curr;
  curr = str.find("\r\n", curr);
  std::string line;
  line = str.substr(last, curr - last);
  last = curr + 2;
  curr = str.rfind("\r\n");
  std::string headers;
  headers = str.substr(last, curr - last);
  std::string body;
  body = str.substr(curr + 2);

  return std::make_tuple(line, headers, body);
}
static auto split_httplike_headers(::std::string_view str) noexcept(true)
  -> std::vector<std::string> {
  std::vector<std::string> res;

  size_t curr = 0, last = curr;
  while (true) {
    curr = str.find("\r\n", curr);
    if (curr == std::string::npos) {
      break;
    }

    res.emplace_back(str.substr(last, curr - last));
    last = curr + 2;
    curr = last;
  }

  return res;
}
static auto split_httplike_header(::std::string_view header)
  -> std::pair<std::string, std::string> {
  const size_t mid = header.find(':');
  return std::make_pair(
    trim(header.substr(0, mid)), trim(header.substr(mid + 1)));
}

/* converter */
static auto wstring2string(std::wstring_view ws) noexcept(true) -> std::string
{
  std::string str_locale = std::setlocale(LC_ALL, "");
  const wchar_t *wch_src = ws.data();
  size_t n_dest_size = std::wcstombs(nullptr, wch_src, 0) + 1;
  char *ch_dest = new char[n_dest_size];
  ::memset(ch_dest, 0, n_dest_size);
  std::wcstombs(ch_dest, wch_src, n_dest_size);
  std::string str_result{ch_dest};
  delete[] ch_dest;
  std::setlocale(LC_ALL, str_locale.c_str());
  return str_result;
}
static auto string2wstring(std::string_view s) noexcept(true) -> std::wstring
{
  std::string str_locale = std::setlocale(LC_ALL, "");
  const char *ch_src = s.data();
  size_t n_dest_size = std::mbstowcs(nullptr, ch_src, 0) + 1;
  wchar_t *wch_dest = new wchar_t[n_dest_size];
  ::wmemset(wch_dest, 0, n_dest_size);
  std::mbstowcs(wch_dest, ch_src, n_dest_size);
  std::wstring wstr_result{wch_dest};
  delete[] wch_dest;
  std::setlocale(LC_ALL, str_locale.c_str());
  return wstr_result;
}
template <class StringType>
static auto to_cstr(StringType &&s) noexcept(is_any_of_v<std::decay_t<StringType>, std::string, std::string_view, const char *, char *, char *const, const char *const>) -> const char *
{
  using RawStringType = std::decay_t<StringType>;

  if constexpr (std::is_same_v<RawStringType, std::string>) {
    return s.c_str();
  }
  else if constexpr (std::is_same_v<RawStringType, std::string_view>) {
    return s.data();
  }
  else if constexpr (is_any_of_v<RawStringType, const char *, char *, char *const, const char *const>) {
    return s;
  }
}

static auto convert(std::string_view s,
  const std::unordered_map<std::string, std::string> &mappings) noexcept(true) -> std::string
{
  std::string r{s};
  for (auto &[key, value] : mappings) {
    while (true) {
      auto pos = r.find(key);
      if (pos == std::string::npos) break;
      r.replace(pos, key.length(), value);
    }
  }
  return r;
  // R"(\)" => R"(＼)";
  // R"(/\!<>:*?"|\n\t\r\f\v)";
}

} // namespace string_util
