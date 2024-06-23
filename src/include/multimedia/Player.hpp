#pragma once

#include <iostream>
#include <sstream>
#include <string>

#include "multimedia/MediaList.hpp"
#include "multimedia/MediaSource.hpp"
#include "multimedia/common/StringUtil.hpp"
#include "multimedia/common/Bit.hpp"
#include "multimedia/FFmpegUtil.hpp"

#include <yaml-cpp/yaml.h>
#if defined(_WIN32)
# include <Windows.h>
#elif defined(__linux__)
# include <X11/Xlib.h>
#endif
enum class LoopType { NO_LOOP, LOOP_LIST, LOOP_SIGNAL, LOOP_LIST_AND_SIGNAL};
struct PlayerConfig
{
  PlayerConfig() {
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

    video.xleft = (video.max_width - video.width) / 2;
    video.ytop = (video.max_height - video.height) / 2;
  }

  struct video{
    int width{-1}, height{-1};
    AVRational frame_rate{25, 1};

    AVPixelFormat format{AV_PIX_FMT_YUV420P};
    AVRational sample_aspect_ratio{16, 9};

    int xleft{0};
    int ytop{0};
    int max_width{1920};
    int max_height{1080};
    bool keep_raw_ratio{true};
    bool auto_fit{true};

    YAML::Node dump2Yaml() const {
      YAML::Node video;
      video["xleft"] = xleft;
      video["ytop"] = ytop;
      video["width"] = width;
      video["height"] = height;
      video["frame_rate"] = av_q2d(frame_rate);
      video["max_width"] = max_width;
      video["max_height"] = max_height;
      video["keep_raw_ratio"] = keep_raw_ratio;
      video["auto_fit"] = auto_fit;
      video["sample_aspect_ratio"] = av_q2d(sample_aspect_ratio);
      return video;
    }
  }video;
  struct audio{
    int sample_rate{-1};
    int channels{2};
    AVSampleFormat format{AV_SAMPLE_FMT_S16};

    float volume{1.0f};
    bool is_muted{false};

    YAML::Node dump2Yaml() const {
      YAML::Node audio;
      audio["sample_rate"] = sample_rate;
      audio["channels"] = channels;
      audio["volume"] = volume;
      audio["is_muted"] = is_muted;
      return audio;
    }
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
    LoopType loop{LoopType::NO_LOOP};
    Bit auto_read_next_media{false};
    Bit save_while_playing{false};  // 播放设备流网络流时有效
    std::string save_file;
    
    YAML::Node dump2Yaml() const {
      YAML::Node common;
      common["enable_audio"] = enable_audio;
      common["enable_video"] = enable_video;
      common["enable_subtitle"] = enable_subtitle;
      common["seek_step"] = seek_step;
      common["force_idr"] = force_idr;
      common["speed"] = speed;
      common["loop"] = (int)loop;
      common["auto_read_next_media"] = auto_read_next_media.get();
      common["save_while_playing"] = save_while_playing.get();
      return common;
    }
  }common;

  bool debug_on{true};
  bool enable_log{false};

  std::string dump2Yaml() const {
    YAML::Node root;
    root["video"] = video.dump2Yaml();
    root["audio"] = audio.dump2Yaml();
    // root["subtitle"] = subtitle.dump2Yaml();
    root["common"] = common.dump2Yaml();
    root["debug_on"] = debug_on;
    root["enable_log"] = enable_log;
    return (std::ostringstream{} << root).str();
  }

  void setVideoSize(int w, int h) {
    video.width = w;
    video.height = h;
  }
  void setFrameRate(AVRational fps) {
    video.frame_rate = fps;
  }
  void setSampleRate(int rate) {
    audio.sample_rate = rate;
  }
  void setChannels(int channels) {
    audio.channels = channels;
  }
  void setVolume(float volume) {
    audio.volume = volume;
  }
  void setMute(bool mute) {
    audio.is_muted = mute;
  }

  void setSpeed(double speed) {
    common.speed = speed;
  }
  void setSeekStep(int step) {
    common.seek_step = step;
  }
  void setEnableAudio(bool enable) {
    common.enable_audio = enable;
  }
  void setEnableVideo(bool enable) {
    common.enable_video = enable;
  }

  bool isSignalLoop() const { return common.loop == LoopType::LOOP_SIGNAL || common.loop == LoopType::LOOP_LIST_AND_SIGNAL; }
  bool isListLoop() const { return common.loop == LoopType::LOOP_LIST || common.loop == LoopType::LOOP_LIST_AND_SIGNAL; }
  bool isEnableAStream() const { return common.enable_audio; }
  bool isEnableVStream() const { return common.enable_video; }
  bool isEnableAVStream() const { return common.enable_video && common.enable_audio; }

};

class Player
{
public:
  enum PlayerState {
    NONE = 0,
    READY,  // ready, the config params are initialized
    READY2PLAY, // ready to play 
    PLAYING,  // playing
    PAUSED,  // paused
    FINISHED,  // finished
    ABORT,  // error
  };

  static volatile inline bool is_native_mode = true;

  Player() = default;
  virtual ~Player() = default;

  virtual bool init(PlayerConfig config) = 0;
  virtual bool pause() = 0;
  virtual bool replay() = 0;
  virtual void seek(double pos) = 0;
  virtual double getTotalTime() const = 0;
  virtual double getCurrentTime() const = 0;

  void seekPrev() { seek((getCurrentTime() - config_.common.seek_step)); }
  void seekNext() { seek((getCurrentTime() + config_.common.seek_step)); }

  void setVolume(float volume) {
    config_.audio.volume = volume;
  }
  void volumeUp() {
    if (config_.audio.volume >= 1.0f) return;
    config_.audio.volume += 0.1f;
  }
  void volumeDown() {
    if (config_.audio.volume <= 0.0f) return;
    config_.audio.volume -= 0.1f;
  }
  void setMute(bool muted) {
    config_.audio.is_muted = muted;
  }
  void setLoop(LoopType loop) {
    config_.common.loop = loop;
  }
  void setSpeed(float speed) {
    config_.common.speed = speed;
  }
  void setVideoSize(int width, int height) {
    config_.video.width = width;
    config_.video.height = height;
  }

  PlayerConfig config() const { return config_; }
  struct PlayerConfig::audio getAudioInfo() const { return config_.audio; }
  struct PlayerConfig::video getVideoInfo() const { return config_.video; }
  PlayerState getState() const { return state_; }
  bool isPlaying() const { return state_ == PLAYING; }
  bool isPaused() const { return state_ == PAUSED; }
  bool isFinished() const { return state_ == FINISHED; }
  bool isNetworkStream() const { return is_streaming_; }
  bool isEnableAudio() const { return config_.common.enable_audio; }
  bool isEnableVideo() const { return config_.common.enable_video; }
  bool isEnableSubtitle() const { return config_.common.enable_subtitle; }
  bool isEnableAudioAndVideo() const { return isEnableAudio() && isEnableVideo(); }

  void shuttle() { list_.shuttle(); }

protected:
  virtual bool open(const std::string& url) = 0;
  virtual bool openDevice(const std::string& url, const std::string &shortName) = 0;
  virtual bool play() = 0;
  virtual bool close() = 0;
  virtual void destroy() = 0;

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
  PlayerConfig config_;
  Bit is_streaming_{false};
  MediaList list_;
};
