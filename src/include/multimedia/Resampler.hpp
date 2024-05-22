#pragma once

#include "multimedia/FFmpegUtil.hpp"

class Resampler
{
public:
  struct Info{
    int sample_rate;
//    AVChannelLayout ch_layout;
    int channels;
    AVSampleFormat format;
  };
  Resampler();
  ~Resampler();

  bool init(Info in, Info out);
  int resample(AVFramePtr pInFrame, AVFramePtr pOutFrame);

private:
  bool isDirty(Info &out) const;
private:
  Info last_;
  SwrContext *swr_context_{nullptr};
};
