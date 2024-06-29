#pragma once

#include "multimedia/MediaSource.hpp"
#include "multimedia/common/Bit.hpp"
#include "multimedia/common/ConditionVariable.hpp"
#include "multimedia/AVQueue.hpp"
#include "multimedia/AVThread.hpp"
#include "multimedia/recorder/Recorder.hpp"

class FFmpegRecorder : public Recorder
{
public:
  FFmpegRecorder();
  ~FFmpegRecorder();

  bool init(RecorderConfig config) override;
  bool open(const MediaSource &source) override;
  void close() override;
  void record() override;
  void pause() override;
  void resume() override;

private:
  void onRead();
  void onWrite();
  void onAudioFrameDecode();
  void onVideoFrameDecode();
  
  bool openInputStream(const std::string &url, const std::string&shortName);
  bool openOutputStream(const std::string &url);

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
  AVThread read_thread_{"ReadThread"};
  AVThread video_write_thread_{"VideoWriteThread"};
  //AVThread audio_write_thread_{"AudioWriteThread"};
  AVThread write_thread_{"WriteThread"};
  AVThread audio_decode_thread_{"AudioDecodeThread"};
  AVThread video_decode_thread_{"VideoDecodeThread"};

  ConditionVariable cond_read_;
  ConditionVariable cond_write_;
  AVPacketQueue in_packets_;
  AVFrameQueue in_frames_;
  Bit need2pause_;
  bool is_aborted_{false};
  bool need_write_tail_{false};
};

