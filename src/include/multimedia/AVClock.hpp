#pragma once

class AVClock
{
public:
  double get() const { return pts_; }
  void set(double pts) { pts_ = pts; }

  void reset() { pts_ = 0.0f; }
private:
  double pts_{0.0f};
};
