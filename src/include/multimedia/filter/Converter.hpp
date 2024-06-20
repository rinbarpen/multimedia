#pragma once

#include "multimedia/FFmpegUtil.hpp"

class Converter
{
public:
  struct Info{
    int width;
    int height;
    AVPixelFormat format;
  };

  Converter();
  ~Converter();

  bool init(Info in, Info out);
  bool convert(AVFramePtr pInFrame, AVFramePtr pOutFrame);

private:
  bool isDirty(Info &out) const;

private:
  Info last_;
  SwsContext *sws_context_{nullptr};
};
