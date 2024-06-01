#pragma once

#include "multimedia/common/ConditionVariable.hpp"
#include "multimedia/AVQueue.hpp"
#include "multimedia/AVThread.hpp"
#include "multimedia/recorder/Recorder.hpp"

class FFmpegRecorder : public Recorder
{
public:
  FFmpegRecorder() = default;
  ~FFmpegRecorder() = default;

  bool init(RecordConfig config) override;
  bool open(const std::string &url, const std::string &shortName) override;
  void close() override;
  void record() override;
  void stop() override;

private:
  void onRead();
  void onWrite();

  bool openInputStream(const std::string &url, const std::string&shortName);
  bool openOutputStream(const std::string &filename);

private:
  struct AVGroup {
    AVFormatContext *format_context{nullptr};
    AVCodecContext *audio_codec_context{nullptr};
    AVStream *audio_stream{nullptr};
    int audio_stream_index{-1};
    AVCodecContext *video_codec_context{nullptr};
    AVStream *video_stream{nullptr};
    int video_stream_index{-1};

    void cleanup() {
      if (format_context) {
        avformat_free_context(format_context);
        format_context = nullptr;
      }
      if (audio_codec_context) {
        avcodec_free_context(&audio_codec_context);
        audio_codec_context = nullptr;
      }
      if (video_codec_context) {
        avcodec_free_context(&video_codec_context);
        video_codec_context = nullptr;
      }
      video_stream = audio_stream = nullptr;
      video_stream_index = audio_stream_index = -1;
    }
  };

  AVGroup in_, out_;
  AVThread read_thread_;
  AVThread video_write_thread_;
  AVThread audio_write_thread_;
  AVThread write_thread_;
  ConditionVariable cond_read_;
  ConditionVariable cond_write_;
  AVPacketQueue in_packets_;
  bool is_eof_{false};
  bool is_aborted_{false};
  bool need_write_tail_{false};
};

