#include "multimedia/FFmpegPlayer.hpp"

#include <SDL2/SDL.h>
#include <chrono>

#include "multimedia/AudioBuffer.hpp"
#include "multimedia/common/Logger.hpp"
#include "multimedia/common/Time.hpp"

#define AV_SYNC_THRESHOLD_MIN           0.04
#define AV_SYNC_THRESHOLD_MAX           0.1
#define AV_NOSYNC_THRESHOLD             10.0

#define SDL_AUDIO_MIN_BUFFER_SIZE       512
#define SDL_AUDIO_MAX_CALLBACKS_PER_SEC 30

static auto g_FFmpegPlayerLogger = GET_LOGGER3("FFmpegPlayer");

#define FFMPEG_LOG_ERROR(fmt, ...)                              \
  do {                                                          \
    if (config_.enable_log)                                     \
      ILOG_ERROR_FMT(g_FFmpegPlayerLogger, fmt, ##__VA_ARGS__); \
    else                                                        \
      ILOG_ERROR_FMT(g_FFmpegPlayerLogger, fmt, ##__VA_ARGS__); \
  } while (0)

FFmpegPlayer::FFmpegPlayer(AudioDevice audioDevice, VideoDevice videoDevice)
  : audio_device_(audioDevice)
  , video_device_(videoDevice) {
  SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO);
  audio_buffer_.reset(new AudioBuffer(48000 * 4 * 1 * 3));
  openVideo();
}
FFmpegPlayer::~FFmpegPlayer() {
  close();
  closeVideo();
  SDL_Quit();
}

bool FFmpegPlayer::init(PlayerConfig config) {
  if (!check(config)) {
    return false;
  }
  config_ = config;
  state_ = INITED;
  is_eof_ = is_aborted_ = is_streaming_ = false;
  need2seek_ = need2pause_ = false;
  short_name_ = url_ = "";
  return true;
}
bool FFmpegPlayer::close() {
  if (isPlaying()) pause();

  if (isEnableAudio()) closeAudio();

  is_aborted_ = true;
  read_thread_.stop();
  video_decode_thread_.stop();
  audio_decode_thread_.stop();

  if (format_context_) {
    avformat_close_input(&format_context_);
    avformat_free_context(format_context_);
    format_context_ = nullptr;
  }
  if (audio_codec_context_) {
    avcodec_free_context(&audio_codec_context_);
    audio_codec_context_ = nullptr;
  }
  if (video_codec_context_) {
    avcodec_free_context(&video_codec_context_);
    video_codec_context_ = nullptr;
  }

  video_frame_queue_.clear();
  video_packet_queue_.clear();
  audio_frame_queue_.clear();
  audio_packet_queue_.clear();
  audio_stream_index_ = video_stream_index_ = -1;
  audio_stream_ = video_stream_ = nullptr;

  audio_clock_.reset();
  video_clock_.reset();

  is_eof_ = is_aborted_ = is_streaming_ = false;
  need2seek_ = need2pause_ = false;
  short_name_ = url_ = "";
  resampler_.release();
  converter_.release();

  state_ = INITED;
  return true;
}
bool FFmpegPlayer::open(const std::string &url) {
  int r;

  format_context_ = avformat_alloc_context();
  if (!format_context_) {
    FFMPEG_LOG_ERROR("Couldn't allocate format context");
    this->close();
    return false;
  }

  r = avformat_open_input(&format_context_, url.c_str(), nullptr, nullptr);
  if (r < 0) {
    FFMPEG_LOG_ERROR("Couldn't open file");
    this->close();
    return false;
  }
  r = avformat_find_stream_info(format_context_, nullptr);
  if (r < 0) {
    FFMPEG_LOG_ERROR("Couldn't find stream information");
    this->close();
    return false;
  }

  if (config_.debug_on) av_dump_format(format_context_, 0, url.c_str(), 0);

  do {
    if (isEnableAudio()) {
      audio_stream_index_ = av_find_best_stream(
        format_context_, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
      if (audio_stream_index_ < 0) {
        config_.common.enable_audio = false;
        break;
      }
      audio_stream_ = format_context_->streams[audio_stream_index_];
      audio_codec_context_ = avcodec_alloc_context3(nullptr);
      if (!audio_codec_context_) {
        FFMPEG_LOG_ERROR("Couldn't allocate audio codec context");
        this->close();
        return false;
      }
      r = avcodec_parameters_to_context(
        audio_codec_context_, audio_stream_->codecpar);
      if (r < 0) {
        FFMPEG_LOG_ERROR("Couldn't copy audio codec context");
        this->close();
        return false;
      }
      audio_codec_ = avcodec_find_decoder(audio_codec_context_->codec_id);
      if (!audio_codec_) {
        FFMPEG_LOG_ERROR("Couldn't find audio decoder");
        this->close();
        return false;
      }
      r = avcodec_open2(audio_codec_context_, audio_codec_, nullptr);
      if (r < 0) {
        FFMPEG_LOG_ERROR("Couldn't open audio codec");
        this->close();
        return false;
      }
      audio_frame_queue_.clear();
      audio_packet_queue_.clear();
      audio_clock_.reset();

      if (config_.audio.sample_rate <= 0)
        config_.audio.sample_rate = audio_codec_context_->sample_rate;
      size_t maxFrameNum = config_.common.seek_step * config_.audio.sample_rate
                           * config_.audio.channels;
      audio_packet_queue_.setMaxSize(maxFrameNum);
      audio_frame_queue_.setMaxSize(maxFrameNum);
    }
  } while (0);

  do {
    if (isEnableVideo()) {
      video_stream_index_ = av_find_best_stream(
        format_context_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
      if (video_stream_index_ < 0) {
        config_.common.enable_video = false;
        break;
      }
      video_stream_ = format_context_->streams[video_stream_index_];
      video_codec_context_ = avcodec_alloc_context3(nullptr);
      if (!video_codec_context_) {
        FFMPEG_LOG_ERROR("Couldn't allocate video codec context");
        this->close();
        return false;
      }
      r = avcodec_parameters_to_context(
        video_codec_context_, video_stream_->codecpar);
      if (r < 0) {
        FFMPEG_LOG_ERROR("Couldn't copy video codec context");
        this->close();
        return false;
      }
      video_codec_ = avcodec_find_decoder(video_codec_context_->codec_id);
      if (!video_codec_) {
        FFMPEG_LOG_ERROR("Couldn't find video decoder");
        this->close();
        return false;
      }
      r = avcodec_open2(video_codec_context_, video_codec_, nullptr);
      if (r < 0) {
        FFMPEG_LOG_ERROR("Couldn't open video codec");
        this->close();
        return false;
      }
      video_frame_queue_.clear();
      video_packet_queue_.clear();
      video_clock_.reset();

      setWidthAndHeight();
      video_codec_context_->framerate = config_.video.frame_rate =
        video_stream_->avg_frame_rate;

      size_t maxFrameNum = config_.common.seek_step
                           * config_.video.frame_rate.num
                           / config_.video.frame_rate.den;
      video_packet_queue_.setMaxSize(maxFrameNum);
      video_frame_queue_.setMaxSize(maxFrameNum);
    }
  } while (0);

  if (!isEnableAudio() && !isEnableVideo()) {
    ILOG_ERROR_FMT(g_FFmpegPlayerLogger, "No Source to Play!!");
    this->close();
    return false;
  }

  is_streaming_ = isStreamUrl(url_);
  url_ = url;
  if (!openAudio()) config_.common.enable_audio = false;
  if (isEnableVideo()) setWindowSize(config_.video.width, config_.video.height);
  return true;
}
bool FFmpegPlayer::openDevice(
  const std::string &url, const std::string &shortName) {
  int r;

  format_context_ = avformat_alloc_context();
  if (!format_context_) {
    FFMPEG_LOG_ERROR("Couldn't allocate format context");
    this->close();
    return false;
  }

  auto pInputFormat = av_find_input_format(shortName.c_str());
  if (pInputFormat == nullptr) {
    ILOG_ERROR_FMT(g_FFmpegPlayerLogger, "Not found the target device");
    this->close();
    return false;
  }

  AVDictionary *opt = nullptr;
  av_dict_set_int(&opt, "framerate",
    std::lround(av_q2d(config_.video.frame_rate)), AV_DICT_MATCH_CASE);
  av_dict_set_int(&opt, "draw_mouse", 1, AV_DICT_MATCH_CASE);

  r = avformat_open_input(&format_context_, url.c_str(), pInputFormat, &opt);
  if (r < 0) {
    FFMPEG_LOG_ERROR("Couldn't open file");
    this->close();
    return false;
  }
  r = avformat_find_stream_info(format_context_, nullptr);
  if (r < 0) {
    FFMPEG_LOG_ERROR("Couldn't find stream information");
    this->close();
    return false;
  }

  if (config_.debug_on) av_dump_format(format_context_, 0, url.c_str(), 0);

  do {
    if (isEnableAudio()) {
      audio_stream_index_ = av_find_best_stream(
        format_context_, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
      if (audio_stream_index_ < 0) {
        config_.common.enable_audio = false;
        break;
      }
      audio_stream_ = format_context_->streams[audio_stream_index_];
      audio_codec_context_ = avcodec_alloc_context3(nullptr);
      if (!audio_codec_context_) {
        FFMPEG_LOG_ERROR("Couldn't allocate audio codec context");
        this->close();
        return false;
      }
      r = avcodec_parameters_to_context(
        audio_codec_context_, audio_stream_->codecpar);
      if (r < 0) {
        FFMPEG_LOG_ERROR("Couldn't copy audio codec context");
        this->close();
        return false;
      }
      audio_codec_ = avcodec_find_decoder(audio_codec_context_->codec_id);
      if (!audio_codec_) {
        FFMPEG_LOG_ERROR("Couldn't find audio decoder");
        this->close();
        return false;
      }
      r = avcodec_open2(audio_codec_context_, audio_codec_, nullptr);
      if (r < 0) {
        FFMPEG_LOG_ERROR("Couldn't open audio codec");
        this->close();
        return false;
      }
      audio_frame_queue_.clear();
      audio_packet_queue_.clear();
      audio_clock_.reset();

      if (config_.audio.sample_rate)
        config_.audio.sample_rate = audio_codec_context_->sample_rate;
      size_t maxFrameNum = config_.common.seek_step * config_.audio.sample_rate
                           * config_.audio.channels;
      audio_packet_queue_.setMaxSize(maxFrameNum);
      audio_frame_queue_.setMaxSize(maxFrameNum);
    }
  } while (0);

  do {
    if (isEnableVideo()) {
      video_stream_index_ = av_find_best_stream(
        format_context_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
      if (video_stream_index_ < 0) {
        config_.common.enable_video = false;
        break;
      }
      video_stream_ = format_context_->streams[video_stream_index_];
      video_codec_context_ = avcodec_alloc_context3(nullptr);
      if (!video_codec_context_) {
        FFMPEG_LOG_ERROR("Couldn't allocate video codec context");
        this->close();
        return false;
      }
      r = avcodec_parameters_to_context(
        video_codec_context_, video_stream_->codecpar);
      if (r < 0) {
        FFMPEG_LOG_ERROR("Couldn't copy video codec context");
        this->close();
        return false;
      }
      video_codec_ = avcodec_find_decoder(video_codec_context_->codec_id);
      if (!video_codec_) {
        FFMPEG_LOG_ERROR("Couldn't find video decoder");
        this->close();
        return false;
      }
      r = avcodec_open2(video_codec_context_, video_codec_, nullptr);
      if (r < 0) {
        FFMPEG_LOG_ERROR("Couldn't open video codec");
        this->close();
        return false;
      }
      video_frame_queue_.clear();
      video_packet_queue_.clear();
      video_clock_.reset();

      setWidthAndHeight();

      size_t maxFrameNum = config_.common.seek_step
                           * config_.video.frame_rate.num
                           / config_.video.frame_rate.den;
      video_packet_queue_.setMaxSize(maxFrameNum);
      video_frame_queue_.setMaxSize(maxFrameNum);
    }
  } while (0);

  if (!isEnableAudio() && !isEnableVideo()) {
    ILOG_ERROR_FMT(g_FFmpegPlayerLogger, "No Source to Play!!");
    this->close();
    return false;
  }

  url_ = url;
  short_name_ = shortName;
  is_streaming_ = true;
  if (!openAudio()) config_.common.enable_audio = false;
  if (isEnableVideo()) setWindowSize(config_.video.width, config_.video.height);
  return true;
}

bool FFmpegPlayer::play() {
  read_thread_.dispatch(&FFmpegPlayer::onReadFrame, this);
  if (isEnableAudio())
    audio_decode_thread_.dispatch(&FFmpegPlayer::onAudioDecode, this);
  if (isEnableVideo())
    video_decode_thread_.dispatch(&FFmpegPlayer::onVideoDecode, this);

  if (isEnableAudio()) {
    audio_clock_.set(
      audio_stream_->start_time * av_q2d(audio_stream_->time_base));
    SDL_LockAudioDevice(device_id_);
    SDL_PauseAudioDevice(device_id_, 0);
    SDL_UnlockAudioDevice(device_id_);
  }
  if (isEnableVideo()) {
    video_clock_.set(
      video_stream_->start_time * av_q2d(video_stream_->time_base));
    doVideoDisplay();
  }
  else {
    while (!is_aborted_ || !is_eof_)
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  return true;
}

bool FFmpegPlayer::replay() {
  if (isPlaying()) return false;

  need2pause_ = false;
  if (isEnableAudio()) {
    SDL_LockAudioDevice(device_id_);
    SDL_PauseAudioDevice(device_id_, 0);
    SDL_UnlockAudioDevice(device_id_);
  }
  av_read_play(format_context_);
  state_ = PLAYING;
  return true;
}

bool FFmpegPlayer::pause() {
  if (isPaused()) return false;

  need2pause_ = true;
  last_paused_time_ = getCurrentTime();
  if (isEnableAudio()) {
    SDL_LockAudioDevice(device_id_);
    SDL_PauseAudioDevice(device_id_, 1);
    SDL_UnlockAudioDevice(device_id_);
  }
  av_read_pause(format_context_);
  state_ = PAUSED;
  return true;
}

void FFmpegPlayer::stop() {
  close();
}

void FFmpegPlayer::seek(int64_t pos) {
  if (pos < 0)
    pos = 0;
  else if (pos > getTotalTime())
    pos = getTotalTime();
  ILOG_INFO_FMT(g_FFmpegPlayerLogger, "Seek to {}s", pos / AV_TIME_BASE);
  seek_pos_ = pos;
  need2seek_ = true;
}

bool FFmpegPlayer::check(PlayerConfig &config) const {
  if (config.audio.channels <= 0) {
    return false;
  }
  if (config.audio.volume < 0 || config.audio.volume > 5.0f) {
    return false;
  }
  if (config.common.speed <= 0 || config.common.speed > 2.0f) {
    return false;
  }

  return true;
}

void FFmpegPlayer::onReadFrame() {
  int r;
  while (true) {
    if (is_aborted_) return;

    if (need2seek_) {
      pause();
      int64_t seekTarget = seek_pos_;
      r = av_seek_frame(format_context_, -1, seekTarget,
        AVSEEK_FLAG_FRAME | AVSEEK_FLAG_BACKWARD);
      if (r < 0) {
        FFMPEG_LOG_ERROR("Seek to {} failed!", seekTarget / AV_TIME_BASE);
        continue;
      }

      if (isEnableAudio()) {
        audio_packet_queue_.clear();
        audio_frame_queue_.clear();
      }
      if (isEnableVideo()) {
        video_packet_queue_.clear();
        video_frame_queue_.clear();
      }
      need2seek_ = false;
      is_eof_ = false;
      replay();
    }

    continue_read_cond_.waitFor(std::chrono::microseconds(10), [&]() {
      bool audio_is_full = isEnableAudio() && audio_packet_queue_.isFull();
      bool video_is_full = isEnableVideo() && video_packet_queue_.isFull();
      bool is_paused = isPaused();
      if (audio_is_full || video_is_full || is_paused) {
        return false;
      }
      return true;
    });


    auto pPkt = makeAVPacket();
    r = av_read_frame(format_context_, pPkt.get());
    if (r == AVERROR_EOF) {
      ILOG_INFO_FMT(g_FFmpegPlayerLogger, "End of file");
      is_eof_ = true;
      continue_read_cond_.waitFor(
        std::chrono::microseconds(10), [&] { return false; });
      return;
    }
    else if (r < 0) {
      ILOG_WARN_FMT(g_FFmpegPlayerLogger, "Some errors on av_read_frame()");
      continue;
    }

    if (pPkt->stream_index == audio_stream_index_) {
      audio_packet_queue_.push(pPkt);
    }
    else if (pPkt->stream_index == video_stream_index_) {
      video_packet_queue_.push(pPkt);
    }
  }
}
void FFmpegPlayer::onAudioDecode() {
  int r;
  while (!is_aborted_) {
    AVPacketPtr pPkt;
    if (!audio_packet_queue_.pop(pPkt)) {
      continue_read_cond_.signal();
      continue;
    }

    r = avcodec_send_packet(audio_codec_context_, pPkt.get());
    if (r < 0) {
      FFMPEG_LOG_ERROR("Error on sending a packet for decoding");
      break;
    }
    while (true) {
      auto pFrame = makeAVFrame();
      r = avcodec_receive_frame(audio_codec_context_, pFrame.get());
      if (r == AVERROR_EOF || r == AVERROR(EAGAIN))
        break;
      else if (r < 0) {
        FFMPEG_LOG_ERROR("Audio frame may be broken");
        break;
      }

      audio_frame_queue_.push(pFrame);
    }
  }
}
void FFmpegPlayer::onVideoDecode() {
  int r;
  while (!is_aborted_) {
    AVPacketPtr pPkt;
    if (!video_packet_queue_.pop(pPkt)) {
      continue_read_cond_.signal();
      continue;
    }

    r = avcodec_send_packet(video_codec_context_, pPkt.get());
    if (r < 0) {
      FFMPEG_LOG_ERROR("Error on sending a packet for decoding");
      break;
    }
    while (true) {
      auto pFrame = makeAVFrame();
      r = avcodec_receive_frame(video_codec_context_, pFrame.get());
      if (r == AVERROR_EOF || r == AVERROR(EAGAIN))
        break;
      else if (r < 0) {
        FFMPEG_LOG_ERROR("Video frame  may be broken");
        break;
      }

      video_frame_queue_.push(pFrame);
    }
  }
}

int FFmpegPlayer::decodeAudioFrame(AVFramePtr &pOutFrame) {
  AVFramePtr pFrame;
  bool success = audio_frame_queue_.pop(pFrame);
  if (!success) {
    return -1;
  }

  if (!pOutFrame) pOutFrame = makeAVFrame();
  auto tgtFormat = config_.audio.format == AV_SAMPLE_FMT_NONE
                     ? (AVSampleFormat) pFrame->format
                     : config_.audio.format;
  pOutFrame->format = tgtFormat;
  pOutFrame->sample_rate = config_.audio.sample_rate;
  pOutFrame->ch_layout.nb_channels = config_.audio.channels;
  pOutFrame->channel_layout =
    av_get_default_channel_layout(config_.audio.channels);
  Resampler::Info in;
  in.sample_rate = pFrame->sample_rate;
  in.channels = pFrame->ch_layout.nb_channels;
  in.format = (AVSampleFormat) pFrame->format;
  Resampler::Info out;
  out.sample_rate = pFrame->sample_rate;
  out.channels = pFrame->ch_layout.nb_channels;
  out.format = AV_SAMPLE_FMT_S16;

  if (!resampler_) resampler_ = std::make_unique<Resampler>();
  success = resampler_->init(in, out);
  if (!success) return -1;

  auto dataSize = resampler_->resample(pFrame, pOutFrame);
  audio_clock_.set(pFrame->pts * av_q2d(audio_stream_->time_base)
                   + (double) pFrame->nb_samples / pFrame->sample_rate);
  return dataSize;
}
bool FFmpegPlayer::decodeVideoFrame(AVFramePtr &pOutFrame) {
  AVFramePtr pFrame;
  if (!video_frame_queue_.pop(pFrame)) {
    return false;
  }

  last_video_duration_pts_ = pFrame->pts - last_vframe_pts_;
  last_vframe_pts_ = pFrame->pts;

  if (!pOutFrame) pOutFrame = makeAVFrame();
  auto tgtFormat = (config_.video.format == AV_PIX_FMT_NONE)
                     ? (AVPixelFormat) pFrame->format
                     : config_.video.format;
  Converter::Info in;
  in.width = pFrame->width;
  in.height = pFrame->height;
  in.format = (AVPixelFormat) pFrame->format;

  Converter::Info out;
  out.width = config_.video.width;
  out.height = config_.video.height;
  out.format = tgtFormat;

  if (!converter_) converter_ = std::make_unique<Converter>();
  bool success = converter_->init(in, out);
  if (!success) {
    video_clock_.set(pFrame->pts * av_q2d(video_stream_->time_base));
    return false;
  }
  pOutFrame->width = out.width;
  pOutFrame->height = out.height;
  pOutFrame->format = out.format;
  success = converter_->convert(pFrame, pOutFrame);
  if (!success) {
    av_freep(pOutFrame->data);
    video_clock_.set(pFrame->pts * av_q2d(video_stream_->time_base));
    return false;
  }

  video_clock_.set(pFrame->pts * av_q2d(video_stream_->time_base));
  return true;
}

bool FFmpegPlayer::openVideo() {
  if (video_device_ == VideoDevice::SDL) return openSDL(false);
  return false;
}
bool FFmpegPlayer::openAudio() {
  if (audio_device_ == AudioDevice::SDL) return openSDL(true);
  return false;
}
bool FFmpegPlayer::closeVideo() {
  if (video_device_ == VideoDevice::SDL) return closeSDL(false);
  return false;
}
bool FFmpegPlayer::closeAudio() {
  if (audio_device_ == AudioDevice::SDL) return closeSDL(true);
  return false;
}

bool FFmpegPlayer::openSDL(bool isAudio) {
  if (isAudio) {
    // sdl audio fix S16
    config_.audio.format = AV_SAMPLE_FMT_S16;

    SDL_AudioSpec have, wanted;
    memset(&wanted, 0, sizeof(wanted));
    wanted.freq = config_.audio.sample_rate;
    wanted.samples = FFMAX(SDL_AUDIO_MIN_BUFFER_SIZE,
      2 << av_log2(wanted.freq / SDL_AUDIO_MAX_CALLBACKS_PER_SEC));
    wanted.channels = config_.audio.channels;
    wanted.format = cvtFFSampleFmtToSDLSampleFmt(config_.audio.format);
    wanted.silence = 0;
    wanted.callback = &FFmpegPlayer::sdlAudioCallback;
    wanted.userdata = this;

    for (int i = 0; i < SDL_GetNumAudioDevices(0); i++) {
      device_id_ =
        SDL_OpenAudioDevice(SDL_GetAudioDeviceName(i, 0), 0, &wanted, &have, 0);
      if (device_id_ > 0) {
        ILOG_INFO_FMT(g_FFmpegPlayerLogger, "Open audio device: {}",
          SDL_GetAudioDeviceName(i, 0));
        break;
      }
    }
    if (device_id_ == 0) {
      ILOG_WARN_FMT(g_FFmpegPlayerLogger, "Audio Devices are all busy");
      return false;
    }

    auto wanted_channel_layout = av_get_default_channel_layout(
      wanted.channels == have.channels ? wanted.channels : have.channels);

    audio_hw_params.fmt = config_.audio.format;
    audio_hw_params.freq = have.freq;
    audio_hw_params.channel_layout = wanted_channel_layout;
    audio_hw_params.channels = have.channels;
    audio_hw_params.frame_size = av_samples_get_buffer_size(nullptr,
      audio_hw_params.channels, 1, (AVSampleFormat) audio_hw_params.fmt, 1);
    audio_hw_params.bytes_per_sec =
      av_samples_get_buffer_size(nullptr, audio_hw_params.channels,
        audio_hw_params.freq, (AVSampleFormat) audio_hw_params.fmt, 1);
    if (audio_hw_params.bytes_per_sec <= 0 || audio_hw_params.frame_size <= 0) {
      ILOG_ERROR_FMT(
        g_FFmpegPlayerLogger, "av_samples_get_buffer_size failed!");
      SDL_CloseAudioDevice(device_id_);
      return false;
    }
    ILOG_INFO_FMT(g_FFmpegPlayerLogger, "Setup SDL Audio");
    audio_hw_params.buf_size = have.size;
  }
  else {
    window_ = SDL_CreateWindow("SDL Window", config_.video.xleft,
      config_.video.ytop, config_.video.width, config_.video.height,
      SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window_) {
      ILOG_ERROR_FMT(
        g_FFmpegPlayerLogger, "SDL_CreateWindow Error: {}", SDL_GetError());
      return false;
    }
    renderer_ = SDL_CreateRenderer(window_, 0, 0);
    if (!renderer_) {
      ILOG_ERROR_FMT(
        g_FFmpegPlayerLogger, "SDL_CreateRenderer Error: {}", SDL_GetError());
      return false;
    }
    ILOG_INFO_FMT(g_FFmpegPlayerLogger, "Setup SDL Window");
  }
  return true;
}
bool FFmpegPlayer::closeSDL(bool isAudio) {
  if (isAudio) {
    SDL_LockAudioDevice(device_id_);
    SDL_PauseAudioDevice(device_id_, 1);
    SDL_UnlockAudioDevice(device_id_);
    SDL_CloseAudioDevice(device_id_);
  }
  else {
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
  }
  return true;
}
void FFmpegPlayer::setWindowSize(int w, int h) {
  SDL_SetWindowSize(window_, w, h);
}

void FFmpegPlayer::setWidthAndHeight() {
  if (config_.video.keep_raw_ratio) {
    config_.video.height = video_codec_context_->height;    
    config_.video.width = video_codec_context_->width;
    if (config_.video.height > config_.video.max_height) {
      config_.video.height = config_.video.max_height;
      config_.video.width = config_.video.height * config_.video.max_width
                            / config_.video.max_height;
    }
    if (config_.video.width > config_.video.max_width) {
      config_.video.width = config_.video.max_width;
      config_.video.height = config_.video.width * config_.video.max_height
                            / config_.video.max_width;
    }
    setWindowSize(config_.video.width, config_.video.height);
    return;
  }

  if (config_.video.sample_aspect_ratio.num == 0
      || config_.video.sample_aspect_ratio.den == 0)
    config_.video.sample_aspect_ratio =
      video_codec_context_->sample_aspect_ratio;
  
  if (config_.video.auto_fit) {
    if (config_.video.sample_aspect_ratio.num != 0
        && config_.video.sample_aspect_ratio.den != 0) {
      if (config_.video.height <= 0)
        config_.video.height = video_codec_context_->height;
      if (config_.video.width <= 0)
        config_.video.width = video_codec_context_->height
                              * av_q2d(config_.video.sample_aspect_ratio);
    }
    else {
      if (config_.video.height <= 0)
        config_.video.height = video_codec_context_->height;
      if (config_.video.width <= 0)
        config_.video.width = video_codec_context_->width;
    }
  }

  if (config_.video.height > config_.video.max_height) {
    config_.video.height = config_.video.max_height;
    config_.video.width =
      config_.video.height * av_q2d(config_.video.sample_aspect_ratio);
  }
  if (config_.video.width > config_.video.max_width) {
    config_.video.width = config_.video.max_width;
    config_.video.height =
      config_.video.width / av_q2d(config_.video.sample_aspect_ratio);
  }

  setWindowSize(config_.video.width, config_.video.height);
}

void FFmpegPlayer::sdlAudioCallback(void *ptr, Uint8 *stream, int len) {
  reinterpret_cast<FFmpegPlayer *>(ptr)->sdlAudioHandle(stream, len);
}
void FFmpegPlayer::sdlAudioHandle(Uint8 *stream, int len) {
  int len2 = len;

  int len1, size{0};
  bool success;
  bool silent;
  while (len > 0) {
    silent = false;
    if (audio_buffer_->readableBytes() <= 0) {
      AVFramePtr pOutFrame;
      size = decodeAudioFrame(pOutFrame);
      if (size < 0) {
        silent = true;
        audio_buffer_.reset(
          new AudioBuffer(SDL_AUDIO_MIN_BUFFER_SIZE / audio_hw_params.frame_size
                          * audio_hw_params.frame_size));
      }
      else {
        audio_buffer_.reset(new AudioBuffer(size));
      }
      if (size > 0) {
        audio_buffer_->fill(pOutFrame->extended_data[0], size);
        av_freep(pOutFrame->extended_data);
      }
    }

    len1 = audio_buffer_->readableBytes();
    if (len1 > len) len1 = len;
    if (!config_.audio.is_muted && !silent
        && config_.audio.volume * 100 >= SDL_MIX_MAXVOLUME) {
      memcpy(stream, audio_buffer_->peek(), len1);
    }
    else {
      // silent
      memset(stream, 0, len1);
      if (!config_.audio.is_muted) {
        SDL_MixAudioFormat(stream, audio_buffer_->peek(),
          cvtFFSampleFmtToSDLSampleFmt(config_.audio.format), len1,
          config_.audio.volume * 100);
      }
    }

    len -= len1;
    stream += len1;
    audio_buffer_->extract(nullptr, len1);
  }

  audio_clock_.set(
    audio_clock_.get()
    - (double) (2 * audio_hw_params.buf_size + audio_buffer_->readableBytes())
        / audio_hw_params.bytes_per_sec);
}

void FFmpegPlayer::doEventLoop() {
  if (video_device_ == VideoDevice::SDL) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
      case SDL_QUIT:
        closeVideo();
        this->close();
        SDL_Quit();
        exit(0);
      case SDL_KEYDOWN:
        switch (event.key.keysym.sym) {
        case SDLK_SPACE:
        {
          if (isPaused())
            replay();
          else
            pause();
          break;
        }
        case SDLK_ESCAPE:
          this->close();
          SDL_Quit();
          break;
        case SDLK_LEFT: seek((getCurrentTime() - 5.0f) * AV_TIME_BASE); break;
        case SDLK_RIGHT: seek((getCurrentTime() + 5.0f) * AV_TIME_BASE); break;
        }  // Handle Key Events End
      }
    }
    // Extra handle
    double duration = (double) getTotalTime() / AV_TIME_BASE;
    if (duration - getCurrentTime() < 0.5f) {
      is_aborted_ = true;
    }
    // SDL end
  }
}

void FFmpegPlayer::doVideoDelay() {
  auto tb = av_q2d(video_stream_->time_base);
  double delay = last_video_duration_pts_ * tb;
  auto x = video_frame_queue_.peek();
  if (x) {
    delay = (x->pts - last_vframe_pts_) * tb;
    if (delay < 0.0f || delay > 1.0f) {
      delay = x->duration * tb;
    }
  }

  double diff = 0.0f;
  if (isEnableAudioAndVideo()) {
    ILOG_TRACE_FMT(g_FFmpegPlayerLogger, "Audio: {:3f} | Video: {:3f}",
      audio_clock_.get(), video_clock_.get());

    double sync_threshold =
      FFMAX(AV_SYNC_THRESHOLD_MIN, FFMIN(delay, AV_SYNC_THRESHOLD_MAX))
      * AV_TIME_BASE;
    diff = video_clock_.get() - audio_clock_.get();
    if (fabs(diff) < AV_NOSYNC_THRESHOLD) {  // 10 secs
      if (diff <= -sync_threshold)
        delay = FFMAX(0, delay + diff);
      else if (diff >= sync_threshold && delay > AV_SYNC_THRESHOLD_MAX)
        delay = delay + diff;
      else if (diff >= sync_threshold)
        delay = 2 * delay;
    }
  }
  else {
    double elapse = (double) Clock::elapse() / AV_TIME_BASE;
    delay =
      AV_TIME_BASE / av_q2d(config_.video.frame_rate) / config_.common.speed;
    delay = FFMAX(0, delay - elapse);
  }

  // diff = V - A
  if (diff < -0.3f) {
    delay /= 2;
  }
  else if (diff > 0.3f) {
    delay *= 2;
  }

  auto curr = getCurrentTime();
  int minutes = (int) curr / 60;
  double seconds = curr - minutes * 60;
  ILOG_DEBUG_FMT(g_FFmpegPlayerLogger,
    "{}m:{:.3f}s | Delay: {:.3f}s | A-V: {:.3f}s", minutes, seconds, delay,
    -diff);
  Clock::sleep(delay * AV_TIME_BASE);
}

void FFmpegPlayer::doVideoDisplay() {
  int r;
  while (!is_aborted_) {
    doEventLoop();
    if (isPaused()) {
      continue;
    }

    AVFramePtr pOutFrame;
    bool success = decodeVideoFrame(pOutFrame);
    if (!success) {
      if (pOutFrame) doVideoDelay();
      continue;
    }

    if (video_device_ == VideoDevice::SDL) {
      SDL_PixelFormatEnum format = cvtFFPixFmtToSDLPixFmt(config_.video.format);
      SDL_Texture *pTexture = SDL_CreateTexture(renderer_, format,
        SDL_TEXTUREACCESS_STREAMING, config_.video.width, config_.video.height);
      if (pTexture == nullptr) {
        LOG_ERROR_FMT("Failed to create texture while playing");
        av_freep(pOutFrame->data);
        doVideoDelay();
        break;
      }

      if (format == SDL_PIXELFORMAT_YV12 || format == SDL_PIXELFORMAT_IYUV)
        SDL_UpdateYUVTexture(pTexture, nullptr, pOutFrame->data[0],
          pOutFrame->linesize[0], pOutFrame->data[1], pOutFrame->linesize[1],
          pOutFrame->data[2], pOutFrame->linesize[2]);
      else
        SDL_UpdateTexture(
          pTexture, nullptr, pOutFrame->data[0], pOutFrame->linesize[0]);

      SDL_RenderClear(renderer_);
      SDL_RenderCopy(renderer_, pTexture, nullptr, nullptr);
      SDL_RenderPresent(renderer_);
      SDL_DestroyTexture(pTexture);
    }

    av_freep(pOutFrame->data);
    doVideoDelay();
  }
}

SDL_PixelFormatEnum FFmpegPlayer::cvtFFPixFmtToSDLPixFmt(AVPixelFormat format) {
  switch (format) {
  case AV_PIX_FMT_YUVJ420P:
  case AV_PIX_FMT_YUV420P: return SDL_PIXELFORMAT_YV12;
  case AV_PIX_FMT_YUV422P: return SDL_PIXELFORMAT_YUY2;
  case AV_PIX_FMT_YUV444P: return SDL_PIXELFORMAT_IYUV;
  case AV_PIX_FMT_RGB24: return SDL_PIXELFORMAT_RGB24;
  case AV_PIX_FMT_BGR24: return SDL_PIXELFORMAT_BGR24;
  case AV_PIX_FMT_RGBA: return SDL_PIXELFORMAT_RGBA32;
  case AV_PIX_FMT_BGRA: return SDL_PIXELFORMAT_BGRA32;
  case AV_PIX_FMT_ARGB: return SDL_PIXELFORMAT_ARGB32;
  case AV_PIX_FMT_ABGR: return SDL_PIXELFORMAT_ABGR32;
  default: return SDL_PIXELFORMAT_UNKNOWN;
  }
}

int FFmpegPlayer::cvtFFSampleFmtToSDLSampleFmt(AVSampleFormat format) {
  switch (format) {
  case AV_SAMPLE_FMT_S16: return AUDIO_S16SYS;
  case AV_SAMPLE_FMT_S32: return AUDIO_S32SYS;
  case AV_SAMPLE_FMT_FLT: return AUDIO_F32SYS;
  default: return 0;
  }
}
