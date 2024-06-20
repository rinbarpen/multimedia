#include "multimedia/filter/Converter.hpp"

Converter::Converter() {}
Converter::~Converter() {
  if (sws_context_) {
    sws_freeContext(sws_context_);
  }
}

bool Converter::init(Converter::Info in, Converter::Info out) {
  if (!isDirty(out) && !sws_context_) {
    return true;
  }

  if (sws_context_) {
    sws_freeContext(sws_context_);
  }
  sws_context_ = sws_getContext(in.width, in.height, in.format, out.width,
    out.height, out.format, SWS_BICUBIC, nullptr, nullptr, nullptr);

  if (!sws_context_) return false;
  return true;
}

bool Converter::isDirty(Converter::Info &out) const {
  return last_.format != out.format || last_.height != out.height
         || last_.width != out.width;
}
bool Converter::convert(AVFramePtr pInFrame, AVFramePtr pOutFrame) {
  int r = av_image_alloc(pOutFrame->data, pOutFrame->linesize, pOutFrame->width,
    pOutFrame->height, (AVPixelFormat)pOutFrame->format, 1);
  if (r < 0) return false;

  return sws_scale(sws_context_, pInFrame->data, pInFrame->linesize, 0,
           pInFrame->height, pOutFrame->data, pOutFrame->linesize) >= 0;
}
