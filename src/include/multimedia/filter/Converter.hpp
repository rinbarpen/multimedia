#pragma once

#include <memory>
#include "multimedia/filter/Filter.hpp"

class Converter : public Filter
{
public:
  using ptr = std::shared_ptr<Converter>;
  struct Info{
    int width;
    int height;
    AVPixelFormat format;
  };

  Converter();
  ~Converter();

  static Converter::ptr create();

  bool init(Info in, Info out);
  int run(AVFramePtr pInFrame, AVFramePtr pOutFrame);

private:
  bool isDirty(Info &out) const;

private:
  Info last_;
  SwsContext *sws_context_{nullptr};
};
