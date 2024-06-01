#pragma once

#include "multimedia/AudioBuffer.hpp"
#include "multimedia/Converter.hpp"
#include "multimedia/Resampler.hpp"
#include "multimedia/AVClock.hpp"
#include "multimedia/AVThread.hpp"
#include "multimedia/AVQueue.hpp"
#include "multimedia/Player.hpp"
#include "multimedia/common/ConditionVariable.hpp"

#include <SDL2/SDL.h>

enum class AudioDevice{
  SDL,
};
enum class VideoDevice{
  SDL,
  D3D9EX,
  OPENGL,
  X11,
};

class FFmpegPlayer : public Player
{
  friend class AVReadThread;
  friend class AVVideoDecodeThread;
  friend class AVAudioDecodeThread;
public:
  FFmpegPlayer(AudioDevice audioDevice, VideoDevice videoDevice);
  ~FFmpegPlayer();

  bool init(PlayerConfig config) override;
  bool open(const std::string& url) override;
  bool openDevice(const std::string &url, const std::string& shortName) override;
  bool play() override;
  bool replay() override;
  bool pause() override;
  void stop() override;
  bool close() override;
  void seek(int64_t pos) override;
  int64_t getTotalTime() const override { return format_context_->duration; }
  double getCurrentTime() const override { return audio_clock_.get(); }
  bool isFinished() const { return is_eof_; }
  bool isAborted() const { return is_aborted_; }

protected:
  virtual void doEventLoop();
  virtual void doVideoDisplay();
  virtual void doVideoDelay();

private:
  bool check(PlayerConfig &config) const;

  void onReadFrame();
  void onAudioDecode();
  void onVideoDecode();

  int decodeAudioFrame(AVFramePtr &pOutFrame);
  bool decodeVideoFrame(AVFramePtr &pOutFrame);

  bool openVideo();
  bool openAudio();
  bool closeVideo();
  bool closeAudio();

  bool openSDL(bool isAudio);
  bool closeSDL(bool isAudio);
  void setWindowSize(int w, int h);

  static void sdlAudioCallback(void *ptr, Uint8 *stream, int len);
  void sdlAudioHandle(Uint8 *stream, int len);

  static SDL_PixelFormatEnum cvtFFPixFmtToSDLPixFmt(AVPixelFormat format);

private:
  AVFormatContext *format_context_;
  // audio
  AVCodecContext *audio_codec_context_;
  int audio_stream_index_{-1};
  AVStream *audio_stream_;
  const AVCodec *audio_codec_;
  // video
  AVCodecContext *video_codec_context_;
  int video_stream_index_{-1};
  AVStream *video_stream_;
  const AVCodec *video_codec_;
  // subtitle

  AVFrameQueue video_frame_queue_;
  AVFrameQueue audio_frame_queue_;
  AVPacketQueue video_packet_queue_;
  AVPacketQueue audio_packet_queue_;

  int64_t seek_pos_;
  ConditionVariable continue_read_cond_;

  int64_t last_vframe_pts_{0};
  int64_t last_video_duration_pts_{0};
  AVClock video_clock_;
  AVClock audio_clock_;

  bool need2pause_{false};
  bool need2seek_{false};
  bool is_aborted_{false};
  bool is_eof_{false};

  AVThread read_thread_;
  AVThread video_decode_thread_;
  AVThread audio_decode_thread_;

  std::unique_ptr<Resampler> resampler_;
  std::unique_ptr<Converter> converter_;

  AudioDevice audio_device_;
  VideoDevice video_device_;

  // for SDL
  SDL_Window *window_;
  SDL_Renderer *renderer_;
  SDL_AudioDeviceID device_id_;
  struct AudioParams
  {
    AVSampleFormat fmt;
    int freq;
    uint64_t channel_layout;
    int channels;
    int frame_size;
    int bytes_per_sec;
    int buf_size;
  } audio_hw_params;

  std::unique_ptr<AudioBuffer> audio_buffer_;
};