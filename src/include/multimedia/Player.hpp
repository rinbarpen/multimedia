#pragma once

#include <iostream>
#include <string>

#include "multimedia/common/StringUtil.hpp"
#include "multimedia/FFmpegUtil.hpp"

struct PlayerConfig
{
  struct video{
    int width{-1}, height{-1};
    AVRational frame_rate{25, 1};

    AVPixelFormat format{AV_PIX_FMT_YUV420P};

    int xleft{0};
    int ytop{0};
  }video;
  struct audio{
    int sample_rate{41000};
    int channels{2};
    AVSampleFormat format{AV_SAMPLE_FMT_S16};

    float volume{1.0f};
    bool is_muted{false};
  }audio;
  struct subtitle{
    // Unsupported
  }subtitle;
  struct common{
    bool enable_audio{true};
    bool enable_video{true};
    bool enable_subtitle{true};
    int seek_step = 5; // 5 seconds
    bool force_idr{false};

    float speed{1.0f};
    bool loop{false};
    bool auto_fit{true};
    bool save_while_playing{false};  // 播放设备流时有效
  }common;

  bool debug_on{true};
  bool enable_log{false};
};

class Player
{
public:
  enum PlayerState {
    NONE = 0,
    INITED,  // initialized
    READY,  // ready to play
    PLAYING,  // playing
    PAUSED,  // paused
    ABORT,  // error
  };

  Player() = default;
  virtual ~Player() = default;

  virtual bool init(PlayerConfig config) = 0;
  virtual bool open(const std::string& url) = 0;
  virtual bool openDevice(const std::string& url, const std::string &shortName) = 0;
  virtual bool play() = 0;
  virtual bool replay() = 0;
  virtual bool pause() = 0;
  virtual bool close() = 0;
  virtual void stop() = 0;
  virtual void seek(int64_t pos) = 0;
  virtual int64_t getTotalTime() const = 0;
  virtual double getCurrentTime() const = 0;

  void setVolume(float volume) {
    config_.audio.volume = volume;
  }
  void setMute(bool muted) {
    config_.audio.is_muted = muted;
  }
  void setLoop(bool loop) {
    config_.common.loop = loop;
  }
  void setSpeed(float speed) {
    config_.common.speed = speed;
  }
  void setVideoSize(int width, int height) {
    config_.video.width = width;
    config_.video.height = height;
  }

  struct PlayerConfig::video getVideoInfo() const { return config_.video; }
  struct PlayerConfig::audio getAudioInfo() const { return config_.audio; }
  PlayerState getState() const { return state_; }
  bool isPlaying() const { return state_ == PLAYING; }
  bool isPaused() const { return state_ == PAUSED; }
  bool isNetworkStream() const { return is_streaming_; }

protected:
  static bool isStreamUrl(const std::string& url) {
    if (string_util::start_with(url, "rtsp") || string_util::start_with(url, "rtsps")
        || string_util::start_with(url, "rtmp") || string_util::start_with(url, "rtmps")
        || string_util::start_with(url, "hls")   || string_util::start_with(url, "http")
        || string_util::start_with(url, "https") || string_util::start_with(url, "ws")
        || string_util::start_with(url, "wss")) {
      return true;
    }
    return false;
  }

protected:
  PlayerState state_{NONE};
  std::string url_;
  std::string short_name_;
  bool is_streaming_{false};
  PlayerConfig config_;
};
