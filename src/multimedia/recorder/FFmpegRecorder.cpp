#include <cassert>
#include "multimedia/MediaSource.hpp"
#include "multimedia/common/Logger.hpp"
#include "multimedia/common/OSUtil.hpp"
#include "multimedia/common/Math.hpp"
#include "multimedia/FFmpegUtil.hpp"
#include "multimedia/common/StringUtil.hpp"
#include "multimedia/recorder/FFmpegRecoder.hpp"
#include "multimedia/recorder/Recorder.hpp"


static auto g_FFmpegRecorderLogger = GET_LOGGER3("multimedia.FFmpegRecorder");

FFmpegRecorder::FFmpegRecorder() : Recorder() {}
FFmpegRecorder::~FFmpegRecorder() {
  close();
}

bool FFmpegRecorder::init(RecorderConfig config) {
  config_ = config;
  state_ = READY;
  return true;
}

bool FFmpegRecorder::open(
  const MediaSource &source) {
  if (state_ != READY) {
    close();
    return false;
  }

  if (!openInputStream(source.getUrl(), source.getDeviceName())) {
    close();
    return false;
  }
  output_filename_ = source.getDeviceName() + "_" + source.getUrl();
  //output_filename_ = string_util::convert(output_filename_, { 
  //  {"/", "／"}, 
  //  {"\\", "＼"}, 
  //  {":", "："}, 
  //  {" ", "_"}
  //});
  if (!openOutputStream(source.getUrl())) {
    close();
    return false;
  }

  state_ = READY2RECORD;
  source_ = source;
  return true;
}

void FFmpegRecorder::close() {
  if (state_ <= READY) return;

  if (state_ == RECORDING || state_ == PAUSED) {
    is_aborted_ = true;
    read_thread_.stop();
    if (config_.isEnableAudio()) 
      audio_decode_thread_.stop();
    if (config_.isEnableVideo())
      video_decode_thread_.stop();
    write_thread_.stop();
  }
  if (out_.format_context && out_.format_context->pb) {
    if (need_write_tail_) {
      av_write_trailer(out_.format_context);
      need_write_tail_ = false;
    }
    avio_close(out_.format_context->pb);
  }

  in_.cleanup();
  out_.cleanup();
  output_filename_.clear();

  is_aborted_ = false;
  state_ = READY;
}
void FFmpegRecorder::record() {
  if (state_ == READY2RECORD) {
    if (config_.isEnableAudioAndVideo()) {
      throw 0;
      return;
    }
    read_thread_.dispatch(&FFmpegRecorder::onRead, this);
    if (config_.isEnableAudio()) 
      audio_decode_thread_.dispatch(&FFmpegRecorder::onAudioFrameDecode, this);
    if (config_.isEnableVideo()) 
      video_decode_thread_.dispatch(&FFmpegRecorder::onVideoFrameDecode, this);
    
    write_thread_.dispatch(&FFmpegRecorder::onWrite, this);
    state_ = RECORDING; 
  }
}
void FFmpegRecorder::pause() {
  need2pause_.set();
}
void FFmpegRecorder::resume() {
  need2pause_.unset();
}

void FFmpegRecorder::onRead() {
  int r;
  while (!is_aborted_) {
    if (need2pause_) {
      if (state_ == RECORDING) {
        av_read_pause(in_.format_context);
        state_ = PAUSED;
      }
      else if (state_ == PAUSED) {
        av_read_play(in_.format_context);
        state_ = RECORDING;
      }
      need2pause_.unset();
    }

    auto pPkt = makeAVPacket();
    r = av_read_frame(in_.format_context, pPkt.get());
    if (AVERROR(EAGAIN) == r) {
      continue;
    }
    if (AVERROR_EOF == r) {
      ILOG_INFO_FMT(g_FFmpegRecorderLogger, "read End of Frame");
      break;
    }
    else if (r < 0) {
      ILOG_ERROR_FMT(g_FFmpegRecorderLogger, "av_read_frame() failed");
      break;
    }

    in_packets_.push(pPkt);
  }
}
void FFmpegRecorder::onWrite() {
  int r;

  r = avformat_write_header(out_.format_context, nullptr);
  if (r < 0) {
    ILOG_ERROR_FMT(g_FFmpegRecorderLogger, "avformat_write_header() failed");
    return;
  }
  need_write_tail_ = true;

  auto pInputVideoStream = in_.video_stream;
  auto pOutputVideoStream = out_.video_stream;
  assert(pInputVideoStream && pOutputVideoStream);
  while (!is_aborted_) {
    AVFramePtr pFrame;
    if (!in_frames_.pop(pFrame)) {
      continue;
    }
    
    AVPacketPtr pPkt = makeAVPacket();
    r = avcodec_receive_packet(out_.video_codec_context, pPkt.get());
    if (r == AVERROR_EOF) {
      ILOG_INFO_FMT(g_FFmpegRecorderLogger, "Stream is over");
      break;
    }
    else if (r == AVERROR(EAGAIN)) {
      ILOG_DEBUG_FMT(
        g_FFmpegRecorderLogger, "avcodec_receive_packet() send EAGAIN");
      continue;
    }
    else if (r < 0) {
      ILOG_ERROR_FMT(g_FFmpegRecorderLogger, "avcodec_receive_packet() failed");
      is_aborted_ = true;
      break;
    }

    pPkt->stream_index = out_.video_stream_index;
    pPkt->pts = av_rescale_q_rnd(pFrame->pts, pInputVideoStream->time_base,
      pOutputVideoStream->time_base,
      (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
    pPkt->dts = av_rescale_q_rnd(pFrame->pkt_dts, pInputVideoStream->time_base,
      pOutputVideoStream->time_base,
      (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
    pPkt->duration = av_rescale_q(pFrame->duration,
      pInputVideoStream->time_base, pOutputVideoStream->time_base);
    pPkt->pos = -1;

    r = av_interleaved_write_frame(out_.format_context, pPkt.get());
    if (r < 0) {
      ILOG_ERROR_FMT(g_FFmpegRecorderLogger, "Error on muxing frame");
      is_aborted_ = true;
      break;
    }
  }

  if (need_write_tail_) {
    av_write_trailer(out_.format_context);
    need_write_tail_ = false;
  }
}

void FFmpegRecorder::onAudioFrameDecode() {
  int r;
  while (!is_aborted_) {
    AVPacketPtr pPkt;
    if (!in_packets_.pop(pPkt)) {
      continue;
    }

    r = avcodec_send_packet(in_.audio_codec_context, pPkt.get());
    if (r < 0) {
      ILOG_ERROR_FMT(
        g_FFmpegRecorderLogger, "Error on sending a packet for decoding");
      break;
    }
    while (true) {
      auto pFrame = makeAVFrame();
      r = avcodec_receive_frame(in_.audio_codec_context, pFrame.get());
      if (r == AVERROR_EOF || r == AVERROR(EAGAIN))
        break;
      else if (r < 0) {
        ILOG_ERROR_FMT(g_FFmpegRecorderLogger, "Audio frame may be broken");
        break;
      }

      in_frames_.push(pFrame);
    }
  }
}

void FFmpegRecorder::onVideoFrameDecode() {
  int r;
  while (!is_aborted_) {
    AVPacketPtr pPkt;
    if (!in_packets_.pop(pPkt)) {
      continue;
    }

    r = avcodec_send_packet(in_.video_codec_context, pPkt.get());
    if (r < 0) {
      ILOG_ERROR_FMT(
        g_FFmpegRecorderLogger, "Error on sending a packet for decoding");
      break;
    }
    while (true) {
      auto pFrame = makeAVFrame();
      r = avcodec_receive_frame(in_.video_codec_context, pFrame.get());
      if (r == AVERROR_EOF || r == AVERROR(EAGAIN))
        break;
      else if (r < 0) {
        ILOG_ERROR_FMT(g_FFmpegRecorderLogger, "Video frame  may be broken");
        break;
      }

      in_frames_.push(pFrame);
    }
  }
}

bool FFmpegRecorder::openInputStream(
  const std::string &url, const std::string &shortName) {
  int r;

  in_.format_context = avformat_alloc_context();
  if (!in_.format_context) {
    ILOG_ERROR_FMT(g_FFmpegRecorderLogger, "avformat_alloc_context() failed");
    return false;
  }
  auto pInputFormat = av_find_input_format(shortName.c_str());
  if (pInputFormat == nullptr) {
    ILOG_ERROR_FMT(
      g_FFmpegRecorderLogger, "Cannot find input format: {}", shortName);

    return false;
  }

  AVDictionary *opt = nullptr;
  if (config_.device.is_camera) {}
  else {
    auto &grabber = config_.device.grabber;
    av_dict_set_int(&opt, "framerate", config_.video.frame_rate, AV_DICT_MATCH_CASE);
    av_dict_set_int(&opt, "draw_mouse", grabber.draw_mouse, AV_DICT_MATCH_CASE);
    if (grabber.width > 0 && grabber.height > 0)
      av_dict_set(
        &opt, "video_size", grabber.video_size().c_str(), AV_DICT_MATCH_CASE);
  }

  r = avformat_open_input(
    &in_.format_context, url.c_str(), pInputFormat, &opt);
  if (r < 0) {
    ILOG_ERROR_FMT(g_FFmpegRecorderLogger, "avformat_open_input() failed");

    return false;
  }
  r = avformat_find_stream_info(in_.format_context, nullptr);
  if (r < 0) {
    ILOG_ERROR_FMT(
      g_FFmpegRecorderLogger, "avformat_find_stream_info() failed");

    return false;
  }

  // find valid streams
  in_.audio_stream_index = av_find_best_stream(
    in_.format_context, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
  in_.video_stream_index = av_find_best_stream(
    in_.format_context, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
  config_.common.enable_audio = in_.audio_stream_index >= 0;
  config_.common.enable_video = in_.video_stream_index >= 0;
  if (!config_.isEnableAudio() && !config_.isEnableVideo()) {
    ILOG_ERROR_FMT(g_FFmpegRecorderLogger, "av_find_best_stream() failed");
    return false;
  }

  if (config_.isEnableAudio()) {
    in_.audio_stream = in_.format_context->streams[in_.audio_stream_index];
    in_.audio_codec_context = avcodec_alloc_context3(nullptr);
    if (in_.audio_codec_context == nullptr) {
      ILOG_ERROR_FMT(g_FFmpegRecorderLogger, "avcodec_alloc_context3() failed");
      return false;
    }
    r = avcodec_parameters_to_context(
      in_.audio_codec_context, in_.audio_stream->codecpar);
    if (r < 0) {
      ILOG_ERROR_FMT(
        g_FFmpegRecorderLogger, "avcodec_parameters_to_context() failed");
      return false;
    }
    auto pCodec = avcodec_find_decoder(in_.audio_codec_context->codec_id);
    if (pCodec == nullptr) {
      ILOG_ERROR_FMT(g_FFmpegRecorderLogger, "avcodec_find_decoder() failed");

      return false;
    }
    r = avcodec_open2(in_.audio_codec_context, pCodec, nullptr);
    if (r < 0) {
      ILOG_ERROR_FMT(g_FFmpegRecorderLogger, "avcodec_open2() failed");
      return false;
    }
  }
  if (config_.isEnableVideo()) {
    in_.video_stream = in_.format_context->streams[in_.video_stream_index];
    in_.video_codec_context = avcodec_alloc_context3(nullptr);
    if (in_.video_codec_context == nullptr) {
      ILOG_ERROR_FMT(g_FFmpegRecorderLogger, "avcodec_alloc_context3() failed");
      return false;
    }
    r = avcodec_parameters_to_context(
      in_.video_codec_context, in_.video_stream->codecpar);
    if (r < 0) {
      ILOG_ERROR_FMT(
        g_FFmpegRecorderLogger, "avcodec_parameters_to_context() failed");
      return false;
    }
    auto pCodec = avcodec_find_decoder(in_.video_codec_context->codec_id);
    if (pCodec == nullptr) {
      ILOG_ERROR_FMT(g_FFmpegRecorderLogger, "avcodec_find_decoder() failed");
      return false;
    }
    r = avcodec_open2(in_.video_codec_context, pCodec, nullptr);
    if (r < 0) {
      ILOG_ERROR_FMT(g_FFmpegRecorderLogger, "avcodec_open2() failed");
      return false;
    }

    math_api::window_fit(config_.video.width, config_.video.height,
      in_.video_codec_context->width, in_.video_codec_context->height,
      config_.video.max_width, config_.video.max_height, 1);
  }
  return true;
}
bool FFmpegRecorder::openOutputStream(const std::string &url) {
  int r;

  auto outputRealFilePath = config_.common.output_dir + "/" + output_filename_;
  if (!os_api::exist_file(outputRealFilePath)) {
    os_api::touch(outputRealFilePath);
  }

  r = avformat_alloc_output_context2(
    &out_.format_context, nullptr, nullptr, outputRealFilePath.c_str());
  if (r < 0 || out_.format_context == nullptr) {
    r = avformat_alloc_output_context2(
      &out_.format_context, nullptr, "mpeg", outputRealFilePath.c_str());
    if (r < 0 || out_.format_context == nullptr) {
      ILOG_ERROR_FMT(
        g_FFmpegRecorderLogger, "avformat_alloc_output_context2() failed");
      return false;
    }
  }

  if (config_.isEnableVideo()) {
    auto pCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!pCodec) {
      ILOG_ERROR_FMT(
        g_FFmpegRecorderLogger, "Could not find video encoder for H264");
      return false;
    }

    out_.video_stream = avformat_new_stream(out_.format_context, pCodec);
    if (!out_.video_stream) {
      ILOG_ERROR_FMT(g_FFmpegRecorderLogger, "Could not allocate video stream");
      return false;
    }
    out_.video_stream->time_base = in_.video_stream->time_base;

    out_.video_codec_context = avcodec_alloc_context3(pCodec);
    if (!out_.video_codec_context) {
      ILOG_ERROR_FMT(
        g_FFmpegRecorderLogger, "Could not allocate video codec context");
      return false;
    }

    // Set codec parameters
    out_.video_codec_context->codec_id = pCodec->id;
    out_.video_codec_context->bit_rate = config_.video.bit_rate;
    out_.video_codec_context->width = config_.video.width;
    out_.video_codec_context->height = config_.video.height;
    out_.video_codec_context->time_base =
      AVRational{1, config_.video.frame_rate};
    out_.video_codec_context->framerate =
      AVRational{config_.video.frame_rate, 1};
    out_.video_codec_context->gop_size = config_.video.gop;
    out_.video_codec_context->max_b_frames = 1;
    out_.video_codec_context->pix_fmt = AV_PIX_FMT_YUV420P;

    if (out_.video_codec_context->codec_id == AV_CODEC_ID_H264) {
      av_opt_set(out_.video_codec_context->priv_data, "preset", "ultrafast", 0);
    }

    r = avcodec_open2(out_.video_codec_context, pCodec, nullptr);
    if (r < 0) {
      ILOG_ERROR_FMT(g_FFmpegRecorderLogger,
        "avcodec_open2() failed for video");
      return false;
    }

    // Copy codec parameters to the stream
    r = avcodec_parameters_from_context(
      out_.video_stream->codecpar, out_.video_codec_context);
    if (r < 0) {
      ILOG_ERROR_FMT(g_FFmpegRecorderLogger,
        "avcodec_parameters_from_context() failed for video");
      return false;
    }
  }
  if (config_.isEnableAudio()) {
    auto pCodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!pCodec) {
      ILOG_ERROR_FMT(
        g_FFmpegRecorderLogger, "Could not find audio encoder for ACC");
      return false;
    }

    out_.audio_stream = avformat_new_stream(out_.format_context, nullptr);
    if (!out_.audio_stream) {
      ILOG_ERROR_FMT(g_FFmpegRecorderLogger, "Could not allocate audio stream");
      return false;
    }
    out_.audio_stream->time_base = in_.audio_stream->time_base;

    out_.audio_codec_context = avcodec_alloc_context3(pCodec);
    if (!out_.audio_codec_context) {
      ILOG_ERROR_FMT(
        g_FFmpegRecorderLogger, "Could not allocate audio codec context");
      return false;
    }

    out_.video_codec_context->codec_id = pCodec->id;
    out_.video_codec_context->bit_rate = config_.audio.bit_rate;
    out_.video_codec_context->sample_rate = config_.audio.sample_rate;
    out_.video_codec_context->channels = config_.audio.channels;
    out_.video_codec_context->channel_layout =
      av_get_default_channel_layout(config_.audio.channels);
    out_.video_codec_context->time_base =
      AVRational{1, config_.audio.sample_rate};
    out_.video_codec_context->sample_fmt = AV_SAMPLE_FMT_FLTP;

    r = avcodec_open2(
      out_.audio_codec_context, pCodec, nullptr);
    if (r < 0) {
      ILOG_ERROR_FMT(g_FFmpegRecorderLogger, "avcodec_open2() failed");
      return false;
    }
    r = avcodec_parameters_from_context(
      in_.audio_stream->codecpar, out_.audio_codec_context);
    if (r < 0) {
      ILOG_ERROR_FMT(
        g_FFmpegRecorderLogger, "avcodec_parameters_from_context() failed");
      return false;
    }
  }

  r = avio_open2(&out_.format_context->pb, url.c_str(),
    AVIO_FLAG_WRITE, &out_.format_context->interrupt_callback, nullptr);
  if (r < 0) {
    ILOG_ERROR_FMT(g_FFmpegRecorderLogger, "avio_open2() failed");
    return false;
  }

  return true;
}
