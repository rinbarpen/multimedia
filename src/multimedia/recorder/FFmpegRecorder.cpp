#include "multimedia/common/Logger.hpp"
#include "multimedia/common/OSUtil.hpp"
#include "multimedia/FFmpegUtil.hpp"
#include "multimedia/recorder/FFmpegRecoder.hpp"
#include <cassert>


static auto g_FFmpegRecorderLogger = GET_LOGGER3("multimedia.FFmpegRecorder");

bool FFmpegRecorder::init(RecordConfig config) {
  config_ = config;
  return true;
}

bool FFmpegRecorder::open(
  const std::string &url, const std::string &shortName) {
  if (!openInputStream(url, shortName)) {
    close();
    return false;
  }
  if (!openOutputStream(output_filename_)) {
    close();
    return false;
  }

  state_ = READY;
  url_ = url;
  short_name_ = shortName;
  return true;
}

void FFmpegRecorder::close() {
  if (out_.format_context && out_.format_context->pb) {
    if (need_write_tail_) {
      av_write_trailer(out_.format_context);
      need_write_tail_ = false;
    }
    avio_close(out_.format_context->pb);
  }

  in_.cleanup();
  out_.cleanup();
  output_filename_ = "";

  is_eof_ = is_aborted_ = false;
  state_ = NONE;
}
void FFmpegRecorder::record() {
  write_thread_.dispatch(&FFmpegRecorder::onWrite, this);
}
void FFmpegRecorder::stop() {
  is_aborted_ = true;
  write_thread_.stop();
}


void FFmpegRecorder::onRead() {
  int r;
  while (true) {
    if (is_eof_) {
      break;
    }

    auto pPkt = makeAVPacket();
    r = av_read_frame(in_.format_context, pPkt.get());
    if (AVERROR_EOF == r) {
      avcodec_send_packet(in_.video_codec_context, nullptr);
    }
    else if (0 != r) {
      ILOG_ERROR_FMT(g_FFmpegRecorderLogger, "av_read_frame() failed");
      break;
    }
    else {
      r = avcodec_send_packet(in_.video_codec_context, pPkt.get());
      if (r == AVERROR(EAGAIN)) {}
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
    auto pPkt = makeAVPacket();
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
    pPkt->pts = av_rescale_q_rnd(pPkt->pts, pInputVideoStream->time_base,
      pOutputVideoStream->time_base,
      (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
    pPkt->dts = av_rescale_q_rnd(pPkt->dts, pInputVideoStream->time_base,
      pOutputVideoStream->time_base,
      (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
    pPkt->duration = av_rescale_q(pPkt->duration, pInputVideoStream->time_base, pOutputVideoStream->time_base);
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
  r = avformat_open_input(
    &in_.format_context, url.c_str(), pInputFormat, nullptr);
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
  }
  return true;
}
bool FFmpegRecorder::openOutputStream(const std::string &filename) {
  int r;

  if (filename.empty()) {
    output_filename_ = in_.format_context->url;
  }
  else {
    output_filename_ = filename;
  }
  if (!os_api::exist_dir(config_.common.output_dir)) {
    os_api::mkdir(config_.common.output_dir);
  }
  // if (!os_api::exist_file(output_filename_)) {
  //   os_api::touch(output_filename_);
  // }

  auto outputRealFilePath = config_.common.output_dir + "/" + output_filename_;
  r = avformat_alloc_output_context2(
    &out_.format_context, nullptr, nullptr, outputRealFilePath.c_str());
  if (r < 0 || out_.format_context == nullptr) {
    ILOG_ERROR_FMT(
      g_FFmpegRecorderLogger, "avformat_alloc_output_context2() failed");

    return false;
  }

  if (config_.isEnableVideo()) {
    out_.video_stream = avformat_new_stream(out_.format_context, nullptr);
    if (in_.video_stream->time_base.num == 0) {
      out_.video_stream->time_base = AVRational{config_.video.fps, 1};
    }
    else {
      out_.video_stream->time_base = in_.video_stream->time_base;
    }
    r = avcodec_parameters_from_context(
      out_.video_stream->codecpar, out_.video_codec_context);
    if (r < 0) {
      ILOG_ERROR_FMT(
        g_FFmpegRecorderLogger, "avcodec_parameters_from_context() failed");
      return false;
    }
    
    out_.video_codec_context->pix_fmt = AV_PIX_FMT_YUV420P;
    out_.video_codec_context->time_base = {1, config_.video.fps};
    out_.video_codec_context->framerate = {config_.video.fps, 1};
    out_.video_codec_context->bit_rate = config_.video.bit_rate;
    out_.video_codec_context->gop_size = config_.video.gop;
    out_.video_codec_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    out_.video_codec_context->max_b_frames = 1; // 非B帧之间的最大B帧数(有些格式不支持)
    out_.video_codec_context->qmin = 1;
    out_.video_codec_context->qmax = 5;
    out_.video_codec_context->colorspace = AVCOL_SPC_BT470BG;
    out_.video_codec_context->color_range = AVCOL_RANGE_JPEG;
    out_.video_codec_context->color_primaries = AVCOL_PRI_BT709;

    if (out_.video_codec_context->codec_id == AV_CODEC_ID_H264) {
      av_opt_set(out_.video_codec_context->priv_data, "preset", "ultrafast", 0);
    }

    r = avcodec_open2(out_.video_codec_context,
      avcodec_find_encoder(AV_CODEC_ID_H264), nullptr);
    if (r < 0) {
      ILOG_ERROR_FMT(g_FFmpegRecorderLogger, "avcodec_open2() failed");
      return false;
    }
  }
  if (config_.isEnableAudio()) {
    out_.audio_stream = avformat_new_stream(out_.format_context, nullptr);
    out_.audio_stream->time_base = in_.audio_stream->time_base;
    r = avcodec_parameters_from_context(
      in_.audio_stream->codecpar, out_.audio_codec_context);
    if (r < 0) {
      ILOG_ERROR_FMT(
        g_FFmpegRecorderLogger, "avcodec_parameters_from_context() failed");
      return false;
    }
    r = avcodec_open2(
      out_.audio_codec_context, avcodec_find_encoder(AV_CODEC_ID_AAC), nullptr);
    if (r < 0) {
      ILOG_ERROR_FMT(g_FFmpegRecorderLogger, "avcodec_open2() failed");
      return false;
    }
  }

  r = avio_open2(&out_.format_context->pb, outputRealFilePath.c_str(),
    AVIO_FLAG_WRITE, &out_.format_context->interrupt_callback, nullptr);
  if (r < 0) {
    ILOG_ERROR_FMT(g_FFmpegRecorderLogger, "avio_open2() failed");
    return false;
  }

  return true;
}