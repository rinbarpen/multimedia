#pragma once

#include "multimedia/common/Traits.hpp"

template <typename T>
class Generator
{
  //static_assert(is_numeric_v<T>, "T should be a numric type");
  
public:
  virtual ~Generator() {}

  virtual T get() = 0;
  virtual void reset() = 0;
};

template <typename T>
class ForwardGenerator : public Generator<T>
{
public:
  explicit ForwardGenerator(T defaultValue = 0)
    : default_value_(defaultValue), curr_(defaultValue) {}

  T get() { return curr_++; }
  void reset() { curr_ = default_value_; }

protected:
  T curr_;
  T default_value_;
};
