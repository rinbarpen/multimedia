#pragma once

#include <memory>
#include "multimedia/common/FFmpegUtil.hpp"
class Filter
{
public:
  using ptr = std::shared_ptr<Filter>;

  Filter() = default;
  virtual ~Filter() = default;
  virtual int run(AVFramePtr in, AVFramePtr out) = 0;
};
