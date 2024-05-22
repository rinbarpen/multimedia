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

  is_aborted_ = is_streaming_ = false;
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

      if (config_.video.width < 0)
        config_.video.width = video_codec_context_->width;
      if (config_.video.height < 0)
        config_.video.height = video_codec_context_->height;
      video_codec_context_->framerate = config_.video.frame_rate = video_stream_->avg_frame_rate;
    }
  } while (0);

  if (!config_.common.enable_audio && !config_.common.enable_video) {
    FFMPEG_LOG_ERROR("No Source to Play!!");
    this->close();
    return false;
  }

  is_streaming_ = false;
  return true;
}

void FFmpegPlayer::seek(int64_t pos) {
  seek_pos_ = pos;
  need2seek_ = true;
}
bool FFmpegPlayer::openDevice(
  const std::string &url, const std::string &shortName) {
  return false;
}
bool FFmpegPlayer::check(PlayerConfig &config) const {
  if (config.video.height <= 0 || config.video.width <= 0) {
    return false;
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
    // TODO:
    SDL_AudioSpec have, wanted;
    wanted.samples = 1024;
    wanted.freq = 44100;
    wanted.channels = 2;
    wanted.format = AUDIO_S16;
    wanted.size = 4096;
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

    SDL_LockAudioDevice(device_id_);
    SDL_PauseAudioDevice(device_id_, 0);
    SDL_UnlockAudioDevice(device_id_);
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
void FFmpegPlayer::sdlAudioHandle(Uint8 *stream, int size) {
  int len1;
  if (stream) {

  }
}
