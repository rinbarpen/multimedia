#pragma once

#include "multimedia/AVThread.hpp"
#include "multimedia/common/Generator.hpp"
#include <cstdint>
#include <multimedia/FFmpegUtil.hpp>
#include <multimedia/common/ConditionVariable.hpp>
#include <queue>
#include <string>

struct WriteConfig
{
  struct video
  {
    int width;
    int height;
    int bit_rate;
  } video;
  struct audio
  {
    int channels;
    int sample_rate;
    int bit_rate;
  } audio;

  void setBitRateAuto() const {
    return;
  }
};

class AVWriter
{
public:
  struct AVContextGroup
  {
    AVFormatContext *format_context{};
    AVCodecContext *codec_context{};
    AVStream *stream{};
    int stream_index{-1};
    bool is_video{true};

    void cleanup() {
      if (format_context) {
        avformat_free_context(format_context);
        format_context = nullptr;
      }
      if (codec_context) {
        avcodec_free_context(&codec_context);
        codec_context = nullptr;
      }
      stream = nullptr;
      stream_index = -1;
    }
  };

public:
  enum State
  {
    READY,
    RECORDING,
    //ABORT,
  };

  AVWriter();
  ~AVWriter();

  void open(const std::string &filename, AVContextGroup in, WriteConfig config);
  void close();

  void write(AVFramePtr pFrame);

  void setAsync(bool isAsync);

  bool isOpening() const { return state_ == RECORDING; }

private:
  void onWrite();

  bool openOutputStream(AVFramePtr pFrame);

  void flushAllFrames();
  void writeToFile(AVFramePtr pFrame);
  void writeTailer();

private:
  State state_{READY};
  bool is_aborted_{false};
  WriteConfig config_;
  std::string output_filename_;
  bool need_write_tailer_{false};

  AVContextGroup icg_, ocg_;
  bool is_initialized_ {false};

  bool running_{false};
  std::queue<AVFramePtr> frames_;
  AVThread worker_{ "WriteWorker" };
  Mutex::type mutex_;
  std::condition_variable cond_;
  bool is_async_{false};

  ForwardGenerator<int64_t> pts_{0};
};
