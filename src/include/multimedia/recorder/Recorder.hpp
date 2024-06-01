#pragma once

#include <string>
#include "multimedia/common/noncopyable.hpp"

struct RecordConfig
{
  struct video {
    int width{1920};
    int height{1080};
    int fps{25};
    int bit_rate{800*1000};
    int gop{10};
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

  bool check() const {
    return !common.output_dir.empty();
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
    RECORDING,
    PAUSED,
  };

  Recorder() = default;
  virtual ~Recorder() = default;

  virtual bool init(RecordConfig config) = 0;
  virtual bool open(const std::string &url, const std::string &shortName) = 0;
  virtual void close() = 0;
  virtual void record() = 0;
  virtual void stop() = 0;
  void setOutputDir(const std::string &dir) { config_.common.output_dir = dir; }
  void setOutputFileName(const std::string &filename) { output_filename_ = filename; }
  std::string getOutputFileName() const { return output_filename_; }
  std::string getRecordFilePath() const { return config_.common.output_dir + "/" + output_filename_; }

protected:
  State state_{NONE};
  RecordConfig config_;
  std::string url_;
  std::string short_name_;
  std::string output_filename_;
};
