#include "multimedia/io/AVWriter.hpp"
#include <multimedia/common/OSUtil.hpp>

static auto g_AVWriterLogger = GET_LOGGER3("multimedia.AVWriter");

AVWriter::AVWriter() {}

AVWriter::~AVWriter() {
  if (state_ == RECORDING) close();
}

void AVWriter::open(
  const std::string &filename, AVContextGroup in, WriteConfig config) {
  output_filename_ = filename;
  icg_ = in;
  config_ = config;

  if (is_async_) {
    running_ = true;
    worker_.dispatch(&AVWriter::onWrite, this);
  }
  state_ = RECORDING;
}

void AVWriter::close() {
  if (!is_aborted_) {
    if (is_async_){
      flushAllFrames();
    }

    if (need_write_tailer_) {
      writeTailer();
    }
    avio_closep(&ocg_.format_context->pb);
  }
  else {
    avio_closep(&ocg_.format_context->pb);
    os_api::rm(output_filename_);
  }
  // do not cleanup the icg, icg borrows the external resources
  ocg_.cleanup();
  is_initialized_ = need_write_tailer_ = is_aborted_ = false;
  pts_.reset();
  state_ = READY;
}

void AVWriter::write(AVFramePtr pFrame) {
  if (!is_initialized_) {
    if (openOutputStream(pFrame)) {
      is_initialized_ = true;
      ILOG_INFO_FMT(g_AVWriterLogger, "Open output stream...");
    
      int r = avformat_write_header(ocg_.format_context, nullptr);
      if (r < 0) {
        ILOG_ERROR_FMT(g_AVWriterLogger, "avformat_write_header() failed");
        is_aborted_ = true;
        close();
        return;
      }
      need_write_tailer_ = true;
    }
    else {
      ILOG_ERROR_FMT(g_AVWriterLogger, "openOutputStream() failed");
      is_aborted_ = true;
      close();
      return;
    }
  }

  if (is_async_) {
    Mutex::lock locker(mutex_);
    frames_.push(pFrame);
  }
  else {
    writeToFile(pFrame);
  }
}

void AVWriter::setAsync(bool isAsync) {
  if (isAsync == is_async_) return;

  if (is_async_) {
    flushAllFrames();
  }
  else {
    if (state_ == RECORDING) {
      running_ = true;
      worker_.dispatch(&AVWriter::onWrite, this);
    }
  }
  is_async_ = isAsync;
}

void AVWriter::onWrite() {
  AVFramePtr pFrame;
  while (true) {
    {
      Mutex::ulock locker(mutex_);
      cond_.wait(locker, [this] { 
        return !frames_.empty() || !running_;
      });
      if (frames_.empty()) {
        return ;
      }

      pFrame = frames_.front();
      frames_.pop();
    }
    writeToFile(pFrame);
  }   
}

bool AVWriter::openOutputStream(AVFramePtr pFrame) {
  if (!os_api::exist_file(output_filename_)) {
    os_api::mk(output_filename_, false);
  }

  int r;
  r = avformat_alloc_output_context2(
    &ocg_.format_context, nullptr, nullptr, output_filename_.c_str());
  if (r < 0 || ocg_.format_context == nullptr) {
    r = avformat_alloc_output_context2(
      &ocg_.format_context, nullptr, "mpeg", output_filename_.c_str());
    if (r < 0 || ocg_.format_context == nullptr) {
      ILOG_ERROR_FMT(
        g_AVWriterLogger, "avformat_alloc_output_context2() failed");
      return false;
    }
  }

  auto &pInputFormatContext = icg_.format_context;
  auto &pInputCodecContext = icg_.codec_context;
  auto &pInputStream = icg_.stream;
  auto &pOutputFormatContext = ocg_.format_context;
  auto &pOutputCodecContext = ocg_.codec_context;
  auto &pOutputStream = ocg_.stream;
  if (icg_.is_video) {
    pOutputStream = avformat_new_stream(pOutputFormatContext, nullptr);
    if (!pOutputStream) {
      ILOG_ERROR_FMT(g_AVWriterLogger, "Could not allocate video stream");
      return false;
    }
    ocg_.stream_index = pOutputStream->index;

    auto pCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!pCodec) {
      ILOG_ERROR_FMT(g_AVWriterLogger, "Could not find video encoder for {}", pCodec->name);
      return false;
    }

    pOutputCodecContext = avcodec_alloc_context3(pCodec);
    if (!pOutputCodecContext) {
      ILOG_ERROR_FMT(
        g_AVWriterLogger, "Could not allocate video codec context");
      return false;
    }

    if (pOutputFormatContext->oformat->flags & AVFMT_GLOBALHEADER)
      pOutputCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    pOutputCodecContext->height = pInputCodecContext->height;
    pOutputCodecContext->width = pInputCodecContext->width;
    pOutputCodecContext->sample_aspect_ratio =
      pInputCodecContext->sample_aspect_ratio;
    pOutputCodecContext->pix_fmt = AV_PIX_FMT_YUV420P;
    pOutputCodecContext->time_base = {1, 25};  // TODO: Set your desired frame rate here
    pOutputCodecContext->framerate = {25, 1};
    if (pOutputCodecContext->codec_id == AV_CODEC_ID_H264) {
      av_opt_set(pOutputCodecContext->priv_data, "preset", "ultrafast", 0);
    }

    r = avcodec_open2(pOutputCodecContext, pCodec, nullptr);
    if (r < 0) {
      ILOG_ERROR_FMT(g_AVWriterLogger, "avcodec_open2() failed for video");
      return false;
    }

    r = avcodec_parameters_from_context(
      pOutputStream->codecpar, pOutputCodecContext);
    if (r < 0) {
      ILOG_ERROR_FMT(
        g_AVWriterLogger, "avcodec_parameters_from_context() failed for video");
      return false;
    }
  }
  else {
    auto pCodec = avcodec_find_encoder(pInputCodecContext->codec_id);
    if (!pCodec) {
      ILOG_ERROR_FMT(g_AVWriterLogger, "Could not find audio encoder for ACC");
      return false;
    }

    pOutputStream = avformat_new_stream(pOutputFormatContext, nullptr);
    if (!pOutputStream) {
      ILOG_ERROR_FMT(g_AVWriterLogger, "Could not allocate audio stream");
      return false;
    }
    pOutputStream->time_base = pInputStream->time_base;

    pOutputCodecContext = avcodec_alloc_context3(pCodec);
    if (!pOutputCodecContext) {
      ILOG_ERROR_FMT(
        g_AVWriterLogger, "Could not allocate audio codec context");
      return false;
    }

    r = avcodec_parameters_from_context(
      pInputStream->codecpar, pOutputCodecContext);
    if (r < 0) {
      ILOG_ERROR_FMT(
        g_AVWriterLogger, "avcodec_parameters_from_context() failed");
      return false;
    }

    r = avcodec_open2(pOutputCodecContext, pCodec, nullptr);
    if (r < 0) {
      ILOG_ERROR_FMT(g_AVWriterLogger, "avcodec_open2() failed");
      return false;
    }
  }

  r = avio_open2(&pOutputFormatContext->pb, output_filename_.c_str(),
    AVIO_FLAG_WRITE,
    &pOutputFormatContext->interrupt_callback, nullptr);
  if (r < 0) {
    ILOG_ERROR_FMT(g_AVWriterLogger, "avio_open2() failed");
    return false;
  }

  return true;
}

void AVWriter::flushAllFrames() {
  running_ = false;
  worker_.join();
}

void AVWriter::writeToFile(AVFramePtr pFrame) {
  auto &pOcc = ocg_.codec_context;
  auto &pIcc = icg_.codec_context;

  int r;
  r = avcodec_send_frame(pOcc, pFrame.get());
  if (r < 0) {
    ILOG_ERROR_FMT(g_AVWriterLogger, "avcodec_send_frame() failed");
    running_ = false;
    is_aborted_ = true;
    return ;
  }
  for (;;) {
    auto pPkt = makeAVPacket();
    r = avcodec_receive_packet(pOcc, pPkt.get());
    if (r == AVERROR_EOF) {
      running_ = false;
      return;
    }
    else if (r == AVERROR(EAGAIN)) {
      break;
    }
    else if (r < 0) {
      running_ = false;
      is_aborted_ = true;
      return;
    }

    auto duration = 100'000 / av_q2d(ocg_.codec_context->framerate);

    // 打印调试信息
    //printf("Before rescale: pts=%ld dts=%ld duration=%ld\n", pPkt->pts,
    //  pPkt->dts, pPkt->duration);

    // 使用av_packet_rescale_ts简化时间戳转换
    //av_packet_rescale_ts(
    //  pPkt.get(), icg_.stream->time_base, ocg_.stream->time_base);

    pPkt->dts = pPkt->pts = pts_.get() * duration;
    pPkt->duration = duration;

    // 设置stream_index
    pPkt->stream_index = ocg_.stream->index;

    // 打印转换后的时间戳
    //printf("After rescale: pts=%ld dts=%ld duration=%ld\n", pPkt->pts,
    //  pPkt->dts, pPkt->duration);

    r = av_interleaved_write_frame(ocg_.format_context, pPkt.get());
    if (r < 0) {
      ILOG_ERROR_FMT(g_AVWriterLogger, "av_interleaved_write_frame() failed");
      return;
    }
  }
}

void AVWriter::writeTailer() {
  if (need_write_tailer_) {
    av_write_trailer(ocg_.format_context);
    need_write_tailer_ = false;
    ILOG_INFO_FMT(
      g_AVWriterLogger, "Write the recording file to {}", output_filename_);
  }
}
