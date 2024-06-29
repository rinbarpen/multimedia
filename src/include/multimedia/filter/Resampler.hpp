#pragma once

#include <memory>
#include "multimedia/filter/Filter.hpp"

class Resampler : public Filter {
public:
  using ptr = std::shared_ptr<Resampler>;
  struct Info{
    int sample_rate;
//    AVChannelLayout ch_layout;
    int channels;
    AVSampleFormat format;
  };
  Resampler();
  ~Resampler();

  static Resampler::ptr create();

  bool init(Info in, Info out);
  int run(AVFramePtr pInFrame, AVFramePtr pOutFrame);

private:
  bool isDirty(Info &out) const;
private:
  Info last_;
  SwrContext *swr_context_{nullptr};
};
