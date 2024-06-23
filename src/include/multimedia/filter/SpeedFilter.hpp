#pragma once

#include "Filter.hpp"
#include "multimedia/FFmpegUtil.hpp"

class SpeedFilter : public Filter
{
public:
  using ptr = std::shared_ptr<SpeedFilter>;

  SpeedFilter() = default;
  virtual ~SpeedFilter() = default;

  static SpeedFilter::ptr create(float speed);

  void setSpeed(float speed) { speed_ = speed; }
  float getSpeed() const { return speed_; }

  int run(AVFramePtr in, AVFramePtr out) override;

private:
  float speed_{1.0f};
};
