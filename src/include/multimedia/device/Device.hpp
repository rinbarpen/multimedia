#pragma once

#include <string>
#include <vector>

#include "multimedia/common/FFmpegUtil.hpp"

struct DeviceConfig
{
  struct camera
  {

  } camera;
  struct grabber
  {
    bool draw_mouse{true};
    int offset_x{-1};
    int offset_y{-1};
    int width{-1}, height{-1};

    std::string video_size() const {
      return std::to_string(width) + "x" + std::to_string(height);
    }
  } grabber;

  bool is_camera;
};

class Device
{
public:
  Device(const std::string &name);
  virtual ~Device();

  static std::vector<Device> getDevices();
  static void dump();

  std::string name() const { return name_; }

private:  
  std::string name_;
};
