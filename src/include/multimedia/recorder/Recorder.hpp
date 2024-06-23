#pragma once

#include <string>
#include "multimedia/MediaSource.hpp"
#include "multimedia/common/noncopyable.hpp"
#include "multimedia/common/Math.hpp"

#include <yaml-cpp/yaml.h>
#if defined(_WIN32)
# include <Windows.h>
#elif defined(__linux__)
# include <X11/Xlib.h>
#endif

struct RecorderConfig
{
  RecorderConfig() {
#if defined(_WIN32)
    video.max_width = GetSystemMetrics(SM_CXSCREEN);
    video.max_height = GetSystemMetrics(SM_CYSCREEN);
#elif defined(__linux__)
    auto pXWindow = XOpenDisplay(nullptr);
    if (pXWindow) {
      int screenId = XDefaultScreen(pXWindow);
      video.max_width = XDisplayWidth(pXWindow, screenId);
      video.max_height = XDisplayHeight(pXWindow, screenId);
      XCloseDisplay(pXWindow);
    }
#endif
    video.sample_aspect_ratio = {video.max_width, video.max_height};
  }
  struct video {
    int width{1920};
    int height{1080};
    int frame_rate{25};
    int bit_rate{800*1000};
    int gop{10};
    int max_width;
    int max_height;
    AVRational sample_aspect_ratio;
  } video;
  struct audio {
    int sample_rate{44800};
    int channels{2};
    int bit_rate{20*1000};
  } audio;
  struct common {
    // clip name: ${output_dir}/${input_filename}_${clip_n}_${datetime[%Y-%m-%d]}.mp4
    int max_clip_duration{-1};  //(sec), -1 for no clip
    std::string output_dir{"output"};
    bool force_idr{false};

    bool enable_video{true};
    bool enable_audio{true};
  } common;

  DeviceConfig device;

  bool check() const {
    return !common.output_dir.empty();
  }

  // TODO:
  void setVBitRate() {
    int w = video.width;
    int h = video.height;
  }
  // TODO:
  void setSampleRate() {
  }

  bool isEnableAudioAndVideo() const {
    return common.enable_audio && common.enable_video;
  }
  bool isEnableAudio() const {
    return common.enable_audio;
  }
  bool isEnableVideo() const {
    return common.enable_video;
  }
};

class Recorder : public noncopyable
{
public:
  enum State {
    NONE,
    READY,
    READY2RECORD,
    RECORDING,
    PAUSED,
    FINISHED,
  };

  Recorder() = default;
  virtual ~Recorder() = default;

  virtual bool init(RecorderConfig config) = 0;
  virtual bool open(const MediaSource &source) = 0;
  virtual void close() = 0;
  virtual void record() = 0;
  virtual void pause() = 0;
  virtual void resume() = 0;
  void setOutputDir(const std::string &dir) { config_.common.output_dir = dir; }

  bool isRecording() const { return state_ == RECORDING; }

protected:
  State state_{NONE};
  RecorderConfig config_;
  MediaSource source_;
  std::string output_filename_;
};
