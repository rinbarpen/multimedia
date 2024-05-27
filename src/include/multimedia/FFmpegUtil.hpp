#pragma once

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/time.h>
#include <libavutil/imgutils.h>
#include <libpostproc/postprocess.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include <multimedia/common/Logger.hpp>

static void version() {
  // print ffmpeg version
  ILOG_INFO_FMT(GET_LOGGER2("ffmpeg"),
    "\tavutil: {}\n"
    "\tavcodec: {}\n"
    "\tavformat: {}\n"
    "\tavdevice: {}\n"
    "\tavfilter: {}\n"
    "\tpostprocess: {}\n"
    "\tswresample: {}\n"
    "\tswscale: {}\n",
    avutil_version(), avcodec_version(), avformat_version(), avdevice_version(),
    avfilter_version(), postproc_version(), swresample_version(),
    swscale_version());
}

static void init() {
  avformat_network_init();
}

using AVFramePtr = std::shared_ptr<AVFrame>;
using AVPacketPtr = std::shared_ptr<AVPacket>;

static AVFramePtr makeAVFrame() {
  return AVFramePtr(av_frame_alloc(), [](auto p){
    // if (p->extended_data) av_freep(p->extended_data);
    // if (p->data) av_freep(p->data);
    av_frame_free(&p);
  });
}
static AVPacketPtr makeAVPacket() {
  return AVPacketPtr(av_packet_alloc(), [](auto p){
    av_packet_free(&p);
  });
}
