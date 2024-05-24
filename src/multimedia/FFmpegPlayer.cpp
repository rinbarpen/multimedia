#include "multimedia/FFmpegPlayer.hpp"
#include "multimedia/common/Logger.hpp"

static auto g_FFmpegPlayerLogger = GET_LOGGER2("FFmpegPlayer");

#define FFMPEG_LOG_ERROR(fmt) \
  do {                           \
    if (config_.enable_log) \
      ILOG_ERROR_FMT(g_FFmpegPlayerLogger, fmt": {}", av_err2str(r)); \
    else \
      ILOG_ERROR_FMT(g_FFmpegPlayerLogger, fmt); \
  } while (0)

FFmpegPlayer::FFmpegPlayer(AudioDevice audioDevice, VideoDevice videoDevice)
  : audio_device_(audioDevice), video_device_(videoDevice)
{
}
FFmpegPlayer::~FFmpegPlayer() {
  (void)this->close();
  closeAudio();
  closeVideo();
}

bool FFmpegPlayer::init(PlayerConfig config) {
  if (!check(config)) {
    return false;
  }
  config_ = config;
  state_ = INITED;
  openVideo();
  return true;
}
bool FFmpegPlayer::close() {
  if (isPlaying()) pause();

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
    goto err;
  }

  r = avformat_open_input(&format_context_, url.c_str(), nullptr, nullptr);
  if (r < 0) {
    FFMPEG_LOG_ERROR("Couldn't open file");
    goto err;
  }
  r = avformat_find_stream_info(format_context_, nullptr);
  if (r < 0) {
    FFMPEG_LOG_ERROR("Couldn't find stream information");
    goto err;
  }

  if (config_.debug_on)
    av_dump_format(format_context_, 0, url.c_str(), 0);

  do {
    if (config_.common.enable_audio) {
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
        goto err;
      }
      r = avcodec_parameters_to_context(
        audio_codec_context_, audio_stream_->codecpar);
      if (r < 0) {
        FFMPEG_LOG_ERROR("Couldn't copy audio codec context");
        goto err;
      }
      audio_codec_ = avcodec_find_decoder(audio_codec_context_->codec_id);
      if (!audio_codec_) {
        FFMPEG_LOG_ERROR("Couldn't find audio decoder");
        goto err;
      }
      r = avcodec_open2(audio_codec_context_, audio_codec_, nullptr);
      if (r < 0) {
        FFMPEG_LOG_ERROR("Couldn't open audio codec");
        goto err;
      }
      audio_frame_queue_.clear();
      audio_packet_queue_.clear();
      audio_clock_.reset();

      size_t maxFrameNum = config_.common.seek_step * config_.audio.sample_rate
                           * config_.audio.channels;
      audio_packet_queue_.setMaxSize(maxFrameNum);
      audio_frame_queue_.setMaxSize(maxFrameNum);
    }
  } while (0);

  do {
    if (config_.common.enable_video) {
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
        goto err;
      }
      r = avcodec_parameters_to_context(
        video_codec_context_, video_stream_->codecpar);
      if (r < 0) {
        FFMPEG_LOG_ERROR("Couldn't copy video codec context");
        goto err;
      }
      video_codec_ = avcodec_find_decoder(video_codec_context_->codec_id);
      if (!video_codec_) {
        FFMPEG_LOG_ERROR("Couldn't find video decoder");
        goto err;
      }
      r = avcodec_open2(video_codec_context_, video_codec_, nullptr);
      if (r < 0) {
        FFMPEG_LOG_ERROR("Couldn't open video codec");
        goto err;
      }
      video_frame_queue_.clear();
      video_packet_queue_.clear();
      video_clock_.reset();

      if (config.common.auto_fit) {
        if (config_.video.width <= 0)
          config_.video.width = video_codec_context_->width;
        if (config_.video.height <= 0)
          config_.video.height = video_codec_context_->height;
      }
      video_codec_context_->framerate = config_.video.frame_rate = video_stream_->avg_frame_rate;

      size_t maxFrameNum =
        config_.common.seek_step * config_.video.frame_rate.num / config_.video.frame_rate.den;
      video_packet_queue_.setMaxSize(maxFrameNum);
      video_frame_queue_.setMaxSize(maxFrameNum);
    }
  } while (0);

  if (!config_.common.enable_audio && !config_.common.enable_video) {
    ILOG_ERROR(g_FFmpegPlayerLogger, "No Source to Play!!");
    goto err;
  }

  is_streaming_ = false;
  return true;
err:
  this->close();
  return false;
}
bool FFmpegPlayer::openDevice(
  const std::string &url, const std::string &shortName) {
  int r;

  format_context_ = avformat_alloc_context();
  if (!format_context_) {
    FFMPEG_LOG_ERROR("Couldn't allocate format context");
    goto err;
  }

  auto pInputFormat = av_find_input_format(shortName.c_str());
  if (pInputFormat == nullptr) {
    ILOG_ERROR_FMT(g_FFmpegPlayerLogger, "Not found the target device");
    goto err;
  }

  r = avformat_open_input(&format_context_, url.c_str(), pInputFormat, nullptr);
  if (r < 0) {
    FFMPEG_LOG_ERROR("Couldn't open file");
    goto err;
  }
  r = avformat_find_stream_info(format_context_, nullptr);
  if (r < 0) {
    FFMPEG_LOG_ERROR("Couldn't find stream information");
    goto err;
  }

  if (config_.debug_on) av_dump_format(format_context_, 0, url.c_str(), 0);

  do {
    if (config_.common.enable_audio) {
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
        goto err;
      }
      r = avcodec_parameters_to_context(
        audio_codec_context_, audio_stream_->codecpar);
      if (r < 0) {
        FFMPEG_LOG_ERROR("Couldn't copy audio codec context");
        goto err;
      }
      audio_codec_ = avcodec_find_decoder(audio_codec_context_->codec_id);
      if (!audio_codec_) {
        FFMPEG_LOG_ERROR("Couldn't find audio decoder");
        goto err;
      }
      r = avcodec_open2(audio_codec_context_, audio_codec_, nullptr);
      if (r < 0) {
        FFMPEG_LOG_ERROR("Couldn't open audio codec");
        goto err;
      }
      audio_frame_queue_.clear();
      audio_packet_queue_.clear();
      audio_clock_.reset();

      size_t maxFrameNum = config_.common.seek_step * config_.audio.sample_rate
                           * config_.audio.channels;
      audio_packet_queue_.setMaxSize(maxFrameNum);
      audio_frame_queue_.setMaxSize(maxFrameNum);
    }
  } while (0);

  do {
    if (config_.common.enable_video) {
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
        goto err;
      }
      r = avcodec_parameters_to_context(
        video_codec_context_, video_stream_->codecpar);
      if (r < 0) {
        FFMPEG_LOG_ERROR("Couldn't copy video codec context");
        goto err;
      }
      video_codec_ = avcodec_find_decoder(video_codec_context_->codec_id);
      if (!video_codec_) {
        FFMPEG_LOG_ERROR("Couldn't find video decoder");
        goto err;
      }
      r = avcodec_open2(video_codec_context_, video_codec_, nullptr);
      if (r < 0) {
        FFMPEG_LOG_ERROR("Couldn't open video codec");
        goto err;
      }
      video_frame_queue_.clear();
      video_packet_queue_.clear();
      video_clock_.reset();

      if (config.common.auto_fit) {
        if (config_.video.width <= 0)
          config_.video.width = video_codec_context_->width;
        if (config_.video.height <= 0)
          config_.video.height = video_codec_context_->height;
      }
      video_codec_context_->framerate = config_.video.frame_rate =
        video_stream_->avg_frame_rate;

      size_t maxFrameNum = config_.common.seek_step
                           * config_.video.frame_rate.num
                           / config_.video.frame_rate.den;
      video_packet_queue_.setMaxSize(maxFrameNum);
      video_frame_queue_.setMaxSize(maxFrameNum);
    }
  } while (0);

  if (!config_.common.enable_audio && !config_.common.enable_video) {
    ILOG_ERROR(g_FFmpegPlayerLogger, "No Source to Play!!");
    goto err;
  }

  is_streaming_ = true;
  return true;
err:
  this->close();
  return false;
}

void FFmpegPlayer::seek(int64_t pos) {
  seek_pos_ = pos;
  need2seek_ = true;
}

bool FFmpegPlayer::check(PlayerConfig &config) const {
  if (!config.common.auto_fit) {
    if (config.video.height <= 0 || config.video.width <= 0) {
      return false;
    }
  }
  if (config.audio.sample_rate <= 0 || config.audio.channels <= 0) {
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

void FFmpegPlayer::onReadFrame() 
{
  int r;
  while (true) {
    if (is_aborted_) return;

    if (need2seek_) {
      int64_t seekTarget = seek_pos_;
      r = av_seek_frame(format_context_, -1, seekTarget,
        AVSEEK_FLAG_FRAME | AVSEEK_FLAG_BACKWARD);
      if (r < 0) {
        FFMPEG_LOG_ERROR("Seek to {} failed!", seekTarget / AV_TIMEBASE);
        continue;
      }

      if (enable_audio_) {
        audio_packet_queue_.clear();
        avcodec_flush_buffers(audio_codec_context_);
      }
      if (enable_video_) {
        video_packet_queue_.clear();
        avcodec_flush_buffers(video_codec_context_);
      }
      need2seek_ = false;
      is_eof_ = false;
    }

    continue_read_cond_.waitFor(std::chrono::milliseconds(10), [&]() {
        bool audio_is_full =
        enable_audio_ ? audio_packet_queue_.isFull() : false;
        bool video_is_full =
        enable_video_ ? video_packet_queue_.isFull() : false;
        bool is_paused = isPaused();
        if (audio_is_full || video_is_full || is_paused) {
            return false;
        }
        return true;
    });
    

    auto pPkt = makePacket();
    r = av_read_frame(format_context_, pPkt.get());
    if (r == AVERROR_EOF) {
      ILOG_INFO(g_FFmpegPlayerLogger, "[SDLPlayer] End of file");
      is_eof_ = true;
      continue_read_cond_.waitFor(
        std::chrono::milliseconds(10), [&] { return false; });
      return;
    }
    else if (r < 0) {
      ILOG_WARN(g_FFmpegPlayerLogger, "[SDLPlayer] Some errors on av_read_frame()");
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
void FFmpegPlayer::onAudioDecode() 
{
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
void FFmpegPlayer::onVideoDecode() 
{
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

int FFmpegPlayer::decodeAudioFrame() 
{
  AVFramePtr pFrame;
  bool success = audio_frame_queue_.pop(pFrame);
  if (!success) {
    return -1;
  }
  auto pOutFrame = makeFrame();
  auto tgtFormat = config_.audio.format == AV_SAMPLE_FMT_NONE
                     ? (AVSampleFormat) pFrame->format
                     : config_.audio.format;
  pOutFrame->format = tgtFormat;
  pOutFrame->sample_rate = config_.audio.sample_rate;
  pOutFrame->ch_layout.channels = config_.audio.channels;
  pOutFrame->channel_layout =
    av_get_default_channel_layout(config_.audio.channels);
  Resampler::Info in = {
    .sample_rate = pFrame->sample_rate,
    .channels = pFrame->ch_layout.channels,
    .format = (AVSampleFormat) pFrame->format,
  };
  Resampler::Info out = {
    .sample_rate = config_.audio.sample_rate,
    .channels = config_.audio.channels,
    .format = tgtFormat,
  };
  success = resampler_->init(in, out);
  if (!success) return -1;

  auto dataSize = resampler_->resample(pFrame, pOutFrame);
  audio_clock_.set(
    audio_clock_.get() + (double) pFrame->nb_samples / pFrame->sample_rate);

  return dataSize;
}
int FFmpegPlayer::decodeVideoFrame() {
}

bool FFmpegPlayer::openVideo() {
  if (video_device_ == VideoDevice::SDL)
    openSDL(false);
}
bool FFmpegPlayer::openAudio() {
  if (audio_device_ == AudioDevice::SDL)
    openSDL(true);
}
bool FFmpegPlayer::closeVideo() {
  if (video_device_ == VideoDevice::SDL)
    return closeSDL(false);
  return false;
}
bool FFmpegPlayer::closeAudio() {
  if (audio_device_ == AudioDevice::SDL)
    return closeSDL(true);
  return false;
}

bool FFmpegPlayer::openSDL(bool isAudio) {
  if (isAudio) {
    // sdl audio fix S16
    config_.audio.format = AV_SAMPLE_FMT_S16;
    // TODO:
    SDL_AudioSpec have, wanted;
    wanted.freq = 44100;
    wanted.samples = FFMAX(SDL_AUDIO_MIN_BUFFER_SIZE,
      2 << av_log2(wanted_spec.freq / SDL_AUDIO_MAX_CALLBACKS_PER_SEC));
    wanted.channels = config_.audio.channels;
    wanted.format = AUDIO_S16; // Fixed
    wanted.silence = 0;
    wanted.callback = &FFmpegPlayer::sdlAudioCallback;
    wanted.userdata = this;

    for (int i = 0; i < SDL_GetNumAudioDevices(0); i++) {
      device_id_ = SDL_OpenAudioDevice(SDL_GetAudioDeviceName(i, 0), 0, &wanted, &have, 0);
      if (device_id_ >= 0) {
        ILOG_INFO_FMT(g_FFmpegPlayerLogger, "Open audio device: {}", SDL_GetAudioDeviceName(i, 0));
        break;
      }
    }
    if (device_id_ < 0) {
      ILOG_WARN_FMT(g_FFmpegPlayerLogger , "Audio Devices are all busy");
      return false;
    }

    auto wanted_channel_layout = av_get_default_channel_layout(
      wanted.channels == have.channels ? wanted.channels : have.channels);

    audio_hw_params->fmt = AV_SAMPLE_FMT_S16;
    audio_hw_params->freq = have.freq;
    audio_hw_params->channel_layout = wanted_channel_layout;
    audio_hw_params->channels = have.channels;
    /* audio_hw_params->frame_size这里只是计算一个采样点占用的字节数 */
    audio_hw_params->frame_size = av_samples_get_buffer_size(
      nullptr, audio_hw_params->channels, 1, audio_hw_params->fmt, 1);
    audio_hw_params->bytes_per_sec =
      av_samples_get_buffer_size(nullptr, audio_hw_params->channels,
        audio_hw_params->freq, audio_hw_params->fmt, 1);
    if (audio_hw_params->bytes_per_sec <= 0
        || audio_hw_params->frame_size <= 0) {
      ILOG_ERROR(g_ffmpegPlayerLogger, "av_samples_get_buffer_size failed!");
      SDL_CloseAudioDevice(device_id_);
      return false;
    }
  }
  else {
    window_ = SDL_CreateWindow("SDL Window", config_.video.xleft,
      config_.video.ytop, config_.video.width, config_.video.height, 0);
    if (!window_) {
      ILOG_ERROR_FMT(g_FFmpegPlayerLogger, "SDL_CreateWindow Error: {}", SDL_GetError());
      return false;
    }
    renderer_ = SDL_CreateRenderer(window_, 0, 0);
    if (!renderer_) {
      ILOG_ERROR_FMT(g_FFmpegPlayerLogger, "SDL_CreateRenderer Error: {}", SDL_GetError());
      return false;
    }
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
    SDL_DestroyRenderer(renderer_);
    SDL_DestroyWindow(window_);
  }
  return true;
}
void FFmpegPlayer::sdlAudioCallback(void* ptr, Uint8 *stream, int size) {
  reinterpret_cast<FFmpegPlayer*>(ptr)->sdlAudioHandle(stream, size);
}
void FFmpegPlayer::sdlAudioHandle(Uint8 *stream, int len) {
  if (isPaused()) {
    memset(stream, 0, len);
    return;
  }

  int len1;
  bool success;
  bool silent;
  while (len > 0) {
    silent = false;
    if (audio_buffer_->readableBytes() <= 0) {
      int size = decodeAudioFrame();
      // TODO: optimize this
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
        audio_buffer_->fill(
          pOutFrame->extended_data[0], pOutFrame->linesize[0]);
        //av_freep(&pOutFrame->extended_data[0]);
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
        SDL_MixAudioFormat(stream, audio_buffer_->peek(), AUDIO_S16, len1,
          config_.audio.volume * 100);
      }
    }

    len -= len1;
    stream += len1;
    audio_buffer_->extract(nullptr, len1);
  }

  audio_clock_.set(audio_clock_.get() - 
      (double) (2 * audio_buffer_->capacity() + audio_buffer_->readableBytes()) 
      / audio_hw_params.bytes_per_sec);
}

void FFmpegPlayer::doEventLoop() 
{
  if (video_device_ == VideoDevice::SDL) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
      case SDL_QUIT:
        this->destroy();
        SDL_Quit();
        exit(0);
      case SDL_KEYDOWN:
        switch (event.key.keysym.sym) {
        //case SDLK_SPACE: this->onPauseToggle(); break;
        }
        break;
      }
    }
  }
}

void FFmpegPlayer::doVideoDelay() 
{
  int64_t delay_us = last_video_duration_pts_;
  if (config_.common.enable_audio && config_.common.enable_video) {
    int sync_threshold = FFMAX(0.04f, FFMIN(delay_us, 0.1f)) * AV_TIME_BASE;
    auto diff = video_clock_.get() - audio_clock_.get();
    if (diff < 10 * AV_TIME_BASE) {  // 10 secs
      if (diff <= -sync_threshold) {
        // video is slow
        delay_us = FFMAX(0, delay_us + diff);
      }
      else if (diff >= sync_threshold) {
        auto up = std::ceil(
          AV_TIME_BASE / (config_.video.frame_rate * config_.common.speed));
        if (delayUS > up)
          // video is too fast
          delay_us = delay_us + diff;
        else
          // video is too slow
          delay_us = 2 * delay_us;
      }
    }
  }
  else {
    int64_t elapse_us = Clock::elapse();
    delay_us = FFMAX(0, delay_us - elapse_us);
  }

  Clock::sleep(delay_us);
}

void FFmpegPlayer::doVideoDisplay() 
{
  int r;
  while (!is_aborted_) {
    doEventLoop();
    if (isPaused()) {
      continue;
    }

    AVFramePtr pFrame;
    if (!video_frame_queue_.pop(pFrame)) {
      continue;
    }

    last_video_duration_pts_ = pFrame->pts - last_vframe_pts_; 
    last_vframe_pts_ = pFrame->pts;

    auto pOutFrame = makeFrame();
    auto tgtFormat = (config_.video.format == AV_PIX_FMT_NONE)
                          ? (AVPixelFormat) pFrame->format
                          : config_.video.format;
    converter_->init(pFrame->width, pFrame->height,
      (AVPixelFormat) pFrame->format, config_.video.width, config_.video.height,
      tgtFormat);
    bool success = converter_->convert(pFrame, pOutFrame);
    if (!success) {
      //av_freep(&pOutFrame->data[0]);
      doVideoDelay();
      continue;
    }

    if (video_device_ == VideoDevice::SDL) {
      SDL_PixelFormatEnum format =
        cvtFFPixFmtToSDLPixFmt(config_.video.format);
      SDL_Texture *pTexture = SDL_CreateTexture(renderer_, format,
        SDL_TEXTUREACCESS_STREAMING, config_.video.width, config_.video.height);
      if (pTexture == nullptr) {
        LOG_ERROR("Failed to create texture while playing");
        //av_freep(&pOutFrame->data[0]);
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

    doVideoDelay();
  }
}

SDL_PixelFormatEnum FFmpegPlayer::cvtFFPixFmtToSDLPixFmt(AVPixelFormat format) 
{
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