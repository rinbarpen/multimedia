﻿#include "multimedia/common/AudioBuffer.hpp"
#include "multimedia/common/Logger.hpp"
#include "multimedia/common/Math.hpp"
#include "multimedia/common/Time.hpp"
#include "multimedia/player/FFmpegPlayer.hpp"

#define AV_SYNC_THRESHOLD_MIN           0.04
#define AV_SYNC_THRESHOLD_MAX           0.1
#define AV_NOSYNC_THRESHOLD             10.0

#define SDL_AUDIO_MIN_BUFFER_SIZE       512
#define SDL_AUDIO_MAX_CALLBACKS_PER_SEC 30

static auto g_FFmpegPlayerLogger = GET_LOGGER3("multimedia.FFmpegPlayer");

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
  is_eof_.unset();
  is_aborted_.unset();
  is_streaming_.unset();
  need2seek_.unset();
  need2pause_.unset();
  short_name_ = url_ = "";
  state_ = READY;
  return true;
}
void FFmpegPlayer::destroy() {
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

  resampler_.release();
  converter_.release();

  is_eof_.unset();
  is_aborted_.unset();
  is_streaming_.unset();
  need2seek_.unset();
  need2pause_.unset();
  short_name_ = url_ = "";
}
bool FFmpegPlayer::close() {
  if (state_ <= READY) return false;
  if (isPlaying()) pause();

  if (isEnableAudio()) closeAudio();

  is_aborted_.set();
  if (isEnableAudio()) {
    audio_frame_queue_.close();
    audio_packet_queue_.close();
  }
  if (isEnableVideo()) {
    video_frame_queue_.close();
    video_packet_queue_.close();
  }

  if (!is_native_mode) play_thread_.stop();
  read_thread_.stop();
  if (isEnableAudio()) audio_decode_thread_.stop();
  if (isEnableVideo()) video_decode_thread_.stop();

  if (writer_)
    writer_.release();

  destroy();

  state_ = READY;
  return true;
}
bool FFmpegPlayer::open(
  const std::string &url, const std::string &shortName) {
  if (state_ != READY) {
    if (state_ == NONE) return false;
    close();
  }

  int r;
  format_context_ = avformat_alloc_context();
  if (!format_context_) {
    FFMPEG_LOG_ERROR("Couldn't allocate format context");
    this->destroy();
    return false;
  }

  const AVInputFormat *pInputFormat = nullptr;
  AVDictionary *opt = nullptr;
  if (!shortName.empty()) {
    pInputFormat = av_find_input_format(shortName.c_str());
    if (pInputFormat == nullptr) {
      ILOG_ERROR_FMT(g_FFmpegPlayerLogger, "Not found the target device");
      this->destroy();
      return false;
    }

    if (device_config_.is_camera) {}
    else {
      auto &grabber = device_config_.grabber;
      av_dict_set_int(&opt, "framerate",
        std::lround(av_q2d(config_.video.frame_rate)), AV_DICT_MATCH_CASE);
      av_dict_set_int(&opt, "draw_mouse", grabber.draw_mouse, AV_DICT_MATCH_CASE);
      if (grabber.width > 0 && grabber.height > 0)
        av_dict_set(
          &opt, "video_size", grabber.video_size().c_str(), AV_DICT_MATCH_CASE);
    }
  }

  r = avformat_open_input(&format_context_, url.c_str(), pInputFormat, &opt);
  if (r < 0) {
    FFMPEG_LOG_ERROR("Couldn't open file");
    this->destroy();
    return false;
  }
  r = avformat_find_stream_info(format_context_, nullptr);
  if (r < 0) {
    FFMPEG_LOG_ERROR("Couldn't find stream information");
    this->destroy();
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
        this->destroy();
        return false;
      }
      r = avcodec_parameters_to_context(
        audio_codec_context_, audio_stream_->codecpar);
      if (r < 0) {
        FFMPEG_LOG_ERROR("Couldn't copy audio codec context");
        this->destroy();
        return false;
      }
      audio_codec_ = avcodec_find_decoder(audio_codec_context_->codec_id);
      if (!audio_codec_) {
        FFMPEG_LOG_ERROR("Couldn't find audio decoder");
        this->destroy();
        return false;
      }
      r = avcodec_open2(audio_codec_context_, audio_codec_, nullptr);
      if (r < 0) {
        FFMPEG_LOG_ERROR("Couldn't open audio codec");
        this->destroy();
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
        this->destroy();
        return false;
      }
      r = avcodec_parameters_to_context(
        video_codec_context_, video_stream_->codecpar);
      if (r < 0) {
        FFMPEG_LOG_ERROR("Couldn't copy video codec context");
        this->destroy();
        return false;
      }
      video_codec_ = avcodec_find_decoder(video_codec_context_->codec_id);
      if (!video_codec_) {
        FFMPEG_LOG_ERROR("Couldn't find video decoder");
        this->destroy();
        return false;
      }
      r = avcodec_open2(video_codec_context_, video_codec_, nullptr);
      if (r < 0) {
        FFMPEG_LOG_ERROR("Couldn't open video codec");
        this->destroy();
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
    this->destroy();
    return false;
  }

  url_ = url;
  short_name_ = shortName;
  if (!shortName.empty()) is_streaming_.set();
  if (!openAudio()) config_.common.enable_audio = false;
  if (isEnableVideo() && is_native_mode) setWindowSize(config_.video.width, config_.video.height);
  state_ = READY2PLAY;
  return true;
}

bool FFmpegPlayer::play() {
  if (state_ != READY2PLAY) return false;

  if (isEnableAudio()) {
    audio_frame_queue_.open();
    audio_packet_queue_.open();
  }
  if (isEnableVideo()) {
    video_frame_queue_.open();
    video_packet_queue_.open();
  }

  if (isNetworkStream()) av_read_play(format_context_);

  read_thread_.dispatch(&FFmpegPlayer::onReadFrame, this);
  if (isEnableAudio())
    audio_decode_thread_.dispatch(&FFmpegPlayer::onAudioDecode, this);
  if (isEnableVideo())
    video_decode_thread_.dispatch(&FFmpegPlayer::onVideoDecode, this);

  state_ = PLAYING;
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
    if (is_native_mode)
      doVideoDisplay();
    else
      play_thread_.dispatch(&FFmpegPlayer::doVideoDisplay, this);
  }
  else {
    if (is_native_mode)
      while (!is_aborted_ || !is_eof_)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  return true;
}

bool FFmpegPlayer::replay() {
  if (isPaused()) {
    if (isEnableAudio()) {
      SDL_LockAudioDevice(device_id_);
      SDL_PauseAudioDevice(device_id_, 0);
      SDL_UnlockAudioDevice(device_id_);
    }
    if (isNetworkStream()) av_read_play(format_context_);
    state_ = PLAYING;
    return true;
  }
  return false;
}

bool FFmpegPlayer::pause() {
  // PLAYING, READY2PLAY
  if (isPlaying() || state_ == READY2PLAY) {
    last_paused_time_ = getCurrentTime();
    if (isEnableAudio()) {
      SDL_LockAudioDevice(device_id_);
      SDL_PauseAudioDevice(device_id_, 1);
      SDL_UnlockAudioDevice(device_id_);
    }
    if (isNetworkStream()) av_read_pause(format_context_);
    state_ = PAUSED;
    return true;
  }
  return false;
}

void FFmpegPlayer::play(const MediaList &list) {
  if (list.isEmpty()) return;

  list_ = list;
  bool success;
  do {
    auto media = list_.current();
    device_config_ = media.config();
    success = open(media.getUrl(), media.getDeviceName());
    if (success) {
      play();
      if (need_move_to_prev_) {
        list_.prev();
        close();
        need_move_to_prev_.unset();
      }
      else if (need_move_to_next_) {
        list_.next();
        close();
        need_move_to_next_.unset();
      }
      else {
        list_.next();
      }
    }
  } while (config_.common.auto_read_next_media);
}
void FFmpegPlayer::play(const MediaSource &media, bool isUseLocal) {
  if (isUseLocal) {
    list_.skipTo(media);
  } else {
    list_.clear();
    list_.add(media);
  }

  bool success;
  do {
    auto media = list_.current();
    device_config_ = media.config();
    success = open(media.getUrl(), media.getDeviceName());
    if (success) play();
    list_.next();
  } while (config_.common.auto_read_next_media);
}

void FFmpegPlayer::playPrev() {
  list_.prev();
  play(list_.current(), true);
}
void FFmpegPlayer::playNext() {
  list_.next();
  play(list_.current(), true);
}

void FFmpegPlayer::onPlayPrev() {
  is_aborted_ = true;
  need_move_to_prev_.set();
}
void FFmpegPlayer::onPlayNext() {
  is_aborted_ = true;
  need_move_to_next_.set();
}

void FFmpegPlayer::seek(double pos) {
  if (pos < 0.0f)
    pos = 0.0f;
  else if (pos > getTotalTime())
    pos = getTotalTime();
  ILOG_INFO_FMT(g_FFmpegPlayerLogger, "Seek to {}s", pos);
  seek_pos_ = pos * AV_TIME_BASE;
  need2seek_.set();
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

void FFmpegPlayer::onSetupRecord() {
  config_.common.save_while_playing = true;
}

void FFmpegPlayer::onSetdownRecord() {
  config_.common.save_while_playing = false;
  writer_->close();
}

void FFmpegPlayer::onReadFrame() {
  int r;
  while (!is_aborted_) {
    if (need2seek_) {
      if (isPlaying()) {
        if (isEnableAudio()) {
          SDL_LockAudioDevice(device_id_);
          SDL_PauseAudioDevice(device_id_, 1);
          SDL_UnlockAudioDevice(device_id_);
        }
      }

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
      need2seek_.unset();
      is_eof_.unset();

      if (isPlaying()) {
        if (isEnableAudio()) {
          SDL_LockAudioDevice(device_id_);
          SDL_PauseAudioDevice(device_id_, 0);
          SDL_UnlockAudioDevice(device_id_);
        }
      }
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
      is_eof_.set();
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
  // pOutFrame->channel_layout =
  // av_get_default_channel_layout(config_.audio.channels);
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

  auto dataSize = resampler_->run(pFrame, pOutFrame);
  audio_clock_.set(pFrame->pts * av_q2d(audio_stream_->time_base)
                   + (double) pFrame->nb_samples / pFrame->sample_rate);
  return dataSize;
}
bool FFmpegPlayer::decodeVideoFrame(AVFramePtr &pOutFrame) {
  AVFramePtr pFrame;
  if (!video_frame_queue_.pop(pFrame)) {
    return false;
  }

  if (is_streaming_ && config_.common.track_mode) {
    int drop = 0;
    AVFramePtr pLastestFrame;
    while (video_frame_queue_.pop(pLastestFrame)) {
      auto tb =av_q2d(video_codec_context_->time_base);
      auto diff = (pLastestFrame->pts - pFrame->pts) * tb;
      if (diff < 3.0f) {
        break;
      }
      drop++;
    }
    if (pLastestFrame) pFrame = pLastestFrame;
    if (drop > 0) ILOG_WARN_FMT(g_FFmpegPlayerLogger, "Drop {} frames", drop);
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
  success = converter_->run(pFrame, pOutFrame) >= 0;
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
      device_id_ = 0;
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
    device_id_ = 0;
  }
  else {
    if (renderer_) {
      SDL_DestroyRenderer(renderer_);
      renderer_ = nullptr;
    }
    if (window_) {
      SDL_DestroyWindow(window_);
      window_ = nullptr;
    }
  }
  return true;
}
void FFmpegPlayer::setWindowSize(int w, int h) {
  SDL_SetWindowSize(window_, w, h);
}

void FFmpegPlayer::setWidthAndHeight() {
  if (config_.video.keep_raw_ratio) {
    math_api::window_fit(config_.video.width, config_.video.height,
      video_codec_context_->width, video_codec_context_->height,
      config_.video.max_width, config_.video.max_height, 1);
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
        close();
        closeVideo();
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
          closeVideo();
          this->close();
          SDL_Quit();
          exit(0);
        case SDLK_LEFT: seekPrev(); break;
        case SDLK_RIGHT: seekNext(); break;
        case SDLK_4: onPlayPrev(); return;
        case SDLK_6: onPlayNext(); return;
        // FIXME: Have memory leak when setting up record
        //case SDLK_7: onSetupRecord(); break;
        //case SDLK_8: onSetdownRecord(); break;
        case SDLK_PLUS:
        case SDLK_UP: volumeUp(); break;
        case SDLK_MINUS:
        case SDLK_DOWN: volumeDown(); break;
        case SDLK_m: setMute(!config_.audio.is_muted); break;
        }  // Handle Key Events End
      }
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
      FFMAX(AV_SYNC_THRESHOLD_MIN, FFMIN(delay, AV_SYNC_THRESHOLD_MAX));
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
    delay = 1.0f / av_q2d(config_.video.frame_rate) / config_.common.speed;
    delay = FFMAX(0, delay - elapse);
  }

  if (!(is_streaming_ && config_.common.track_mode)) {
    auto curr = getCurrentTime();
    int minutes = (int) curr / 60;
    double seconds = curr - minutes * 60;
    ILOG_DEBUG_FMT(g_FFmpegPlayerLogger,
      "{}m:{:.3f}s | Delay: {:.3f}s | A-V: {:.3f}s", minutes, seconds, delay,
      -diff);
    Clock::sleep(delay * AV_TIME_BASE);
  }
}

void FFmpegPlayer::doVideoDisplay() {
  int r;
  while (true) {
    if (is_native_mode) doEventLoop();
    if (!is_streaming_) {
      if (getTotalTime() - getCurrentTime() < 0.3f) {
        is_aborted_ = true;
      }
    }
    if (is_aborted_) break;

    if (isPaused()) {
      continue;
    }

    AVFramePtr pOutFrame;
    bool success = decodeVideoFrame(pOutFrame);
    if (!success) {
      if (pOutFrame) doVideoDelay();
      continue;
    }

    do {
      if (!is_native_mode) {
        // sendVFrame2Queue(pOutFrame);
        continue;
      }
      if (video_device_ == VideoDevice::SDL) {
        SDL_PixelFormatEnum format =
          cvtFFPixFmtToSDLPixFmt(config_.video.format);
        SDL_Texture *pTexture =
          SDL_CreateTexture(renderer_, format, SDL_TEXTUREACCESS_STREAMING,
            config_.video.width, config_.video.height);
        if (pTexture == nullptr) {
          LOG_ERROR_FMT("Failed to create texture while playing");
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
    } while (0);

    if (is_streaming_ && config_.common.save_while_playing) {
      if (!writer_) {
        writer_.reset(new AVWriter());
      }
      if (!writer_->isOpening()) {
        AVWriter::AVContextGroup group;
        group.is_video = config_.isEnableVideo();
        group.stream = video_stream_;
        group.stream_index = video_stream_index_;
        group.codec_context = video_codec_context_;
        group.format_context = format_context_;

        writer_->open(config_.common.save_file, group, {});
      }
      writer_->write(pOutFrame);
    } 

    av_freep(pOutFrame->data);
    doVideoDelay();
  }

  state_ = FINISHED;
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