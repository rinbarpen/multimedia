#include "multimedia/Resampler.hpp"
#include "libavutil/mem.h"

Resampler::Resampler() {}
Resampler::~Resampler() {
  if (swr_context_ && swr_is_initialized(swr_context_)) {
    swr_free(&swr_context_);
    swr_context_ = nullptr;
  }
}

bool Resampler::init(Info in, Info out) {
  if (!isDirty(out) && !swr_context_) {
    return true;
  }

  if (swr_context_ && swr_is_initialized(swr_context_)) {
    swr_free(&swr_context_);
    swr_context_ = nullptr;
  }

  int r{-1};
  AVChannelLayout inChannelLayout = {
    .nb_channels = in.channels,
  };
  AVChannelLayout outChannelLayout = {
    .nb_channels = out.channels,
  };
  r = swr_alloc_set_opts2(&swr_context_, &outChannelLayout, out.format,
    out.sample_rate, &inChannelLayout, in.format, in.sample_rate, 0, nullptr);
  if (!swr_context_ || r < 0) return false;

  r = swr_init(swr_context_);
  if (r < 0) {
    swr_free(&swr_context_);
    swr_context_ = nullptr;
    return false;
  }
  last_ = out;
  return true;
}

int Resampler::resample(AVFramePtr pInFrame, AVFramePtr pOutFrame) {
  int64_t outCount = (int64_t) pInFrame->nb_samples * pOutFrame->sample_rate
                       / pInFrame->sample_rate
                     + 256;
  int outSize =
    av_samples_get_buffer_size(nullptr, pOutFrame->ch_layout.nb_channels,
      outCount, (AVSampleFormat) pOutFrame->format, 1);
  if (outSize < 0) return -1;

  uint32_t size = 500 * 1000;
  pOutFrame->extended_data[0] = (uint8_t*)av_malloc(size);
  av_fast_malloc(&pOutFrame->extended_data[0], &size, outSize);
  if (!pOutFrame->extended_data[0]) return false;

  int len = swr_convert(swr_context_, &pOutFrame->extended_data[0], outCount,
    (const uint8_t **) pInFrame->extended_data, pInFrame->nb_samples);

  if (len <= 0 || len == outCount) {
    av_freep(pOutFrame->extended_data);
    return 0;
  }

  return len * pOutFrame->ch_layout.nb_channels
         * av_get_bytes_per_sample((AVSampleFormat) pOutFrame->format);
}

bool Resampler::isDirty(Info &out) const {
  return last_.channels != out.channels || last_.format != out.format
         || last_.sample_rate != out.sample_rate;
}