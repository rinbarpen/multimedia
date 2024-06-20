#pragma once

#include <string>

class MediaSource
{
public:
  MediaSource() = default;
  MediaSource(const std::string &url) 
    : url_(url) {}
  MediaSource(const std::string &url, const std::string &shortName) 
    : url_(url), short_name_(shortName) {}
  ~MediaSource() = default;

  void setUrl(const std::string &url) { url_ = url; }
  std::string getUrl() const { return url_; }
  
  void setDeviceName(const std::string &shortName) { short_name_ = shortName; }
  std::string getDeviceName() const { return short_name_; }

private:
  std::string url_;
  std::string short_name_;
};
