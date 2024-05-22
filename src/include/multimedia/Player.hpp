#pragma once

#include <iostream>
#include <string>

#include "multimedia/FFmpegUtil.hpp"

struct PlayerConfig
{
  struct video{
    int width{1280}, height{720};
    AVRational frame_rate{25, 1};

    int xleft{0};
    int ytop{0};
  }video;
  struct audio{
    int sample_rate;
    int channels;

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
  virtual bool pause() = 0;
  virtual bool close() = 0;
  virtual void stop() = 0;
  virtual void seek(int64_t pos) = 0;
  virtual int64_t getDuration() const = 0;
  virtual int64_t getPosition() const = 0;
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
  PlayerState state_{NONE};
  std::string url_;
  std::string shortName_;
  bool is_streaming_{false};
  PlayerConfig config_;
};
