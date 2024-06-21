#pragma once

#include <string>
#include <vector>

#include "multimedia/FFmpegUtil.hpp"

struct DeviceConfig
{
  struct camera
  {

  } camera;
  struct grabber
  {
    bool draw_mouse{true};
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
