#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "multimedia/common/ConditionVariable.hpp"
#include "multimedia/AVClock.hpp"
#include "multimedia/AVQueue.hpp"
#include "multimedia/AVThread.hpp"
#include "multimedia/AudioBuffer.hpp"
#include "multimedia/Player.hpp"
#include "multimedia/MediaList.hpp"
#include "multimedia/filter/Resampler.hpp"
#include "multimedia/filter/Converter.hpp"
#include "multimedia/io/AVWriter.hpp"

#include <SDL2/SDL.h>

enum class AudioDevice
{
  SDL,
};
enum class VideoDevice
{
  SDL,
  D3D9EX,
  OPENGL,
  X11,
};

class FFmpegPlayer : public Player
{
public:
  FFmpegPlayer(AudioDevice audioDevice, VideoDevice videoDevice);
  ~FFmpegPlayer();

  bool init(PlayerConfig config) override;
  bool replay() override;
  bool pause() override;
  void seek(double pos) override;
  double getTotalTime() const override { return (double)format_context_->duration / AV_TIME_BASE; }
  double getCurrentTime() const override { return audio_clock_.get(); }
  bool isAborted() const { return is_aborted_; }

  void play(const MediaList &list); 
  void play(const MediaSource &media, bool isUseLocal = false);

  void playPrev();
  void playNext();

  size_t getCurrentIndex() const { return list_.currentIndex(); }
  MediaSource getCurrentMediaSource() const {return list_.current(); }
  MediaList getMediaList() const { return list_; }

protected:
  bool open(
    const std::string &url, const std::string &shortName = "") override;
  bool play() override;
  bool close() override;
  virtual void doEventLoop();
  virtual void doVideoDisplay();
  virtual void doVideoDelay();

private:
  void destroy() override;
  bool check(PlayerConfig &config) const;

  void onSetupRecord();
  void onSetdownRecord();
  void onPlayPrev();
  void onPlayNext();
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
  void setWidthAndHeight();

  static void sdlAudioCallback(void *ptr, Uint8 *stream, int len);
  void sdlAudioHandle(Uint8 *stream, int len);

  static SDL_PixelFormatEnum cvtFFPixFmtToSDLPixFmt(AVPixelFormat format);
  static int cvtFFSampleFmtToSDLSampleFmt(AVSampleFormat format);

private:
  AVFormatContext *format_context_{nullptr};
  // audio
  AVCodecContext *audio_codec_context_{nullptr};
  int audio_stream_index_{-1};
  AVStream *audio_stream_{nullptr};
  const AVCodec *audio_codec_{nullptr};
  // video
  AVCodecContext *video_codec_context_{nullptr};
  int video_stream_index_{-1};
  AVStream *video_stream_{nullptr};
  const AVCodec *video_codec_{nullptr};
  // subtitle

  AVFrameQueue video_frame_queue_;
  AVFrameQueue audio_frame_queue_;
  AVPacketQueue video_packet_queue_;
  AVPacketQueue audio_packet_queue_;

  int64_t seek_pos_;
  double last_paused_time_{-1.0f};
  ConditionVariable continue_read_cond_;

  int64_t last_vframe_pts_{0};
  int64_t last_video_duration_pts_{0};
  AVClock video_clock_;
  AVClock audio_clock_;

  Bit need_move_to_prev_;
  Bit need_move_to_next_;
  Bit need2pause_{false};
  Bit need2seek_{false};
  Bit is_aborted_{false};
  Bit is_eof_{false};

  AVThread read_thread_{"ReadThread"};
  AVThread audio_decode_thread_{"AudioDecodeThread"};
  AVThread video_decode_thread_{"VideoDecodeThread"};
  AVThread play_thread_{"PlayThread"};  

  std::unique_ptr <AVWriter> writer_;

  std::unique_ptr<Resampler> resampler_;
  std::unique_ptr<Converter> converter_;

  AudioDevice audio_device_;
  VideoDevice video_device_;
  DeviceConfig device_config_;

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

