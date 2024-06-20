#pragma once

#include <cstdint>
#include <vector>
#include "multimedia/common/Bit.hpp"

class Bitmap
{
public:
  Bitmap(uint16_t n = 1) {
    bits_.resize(n);
  }
  ~Bitmap() = default;

  void set(uint16_t n) {
    bits_[n].set();
  }
  void reset(uint16_t n) {
    bits_[n].set();
  }
  bool get(uint16_t n) {
    return bits_[n].get();
  } 

  void setRange(uint16_t begin, uint16_t end) {
    for (uint16_t i = begin; i < end; i++)
      bits_[begin].set();
  }
  void resetRange(uint16_t begin, uint16_t end) {
    for (uint16_t i = begin; i < end; i++)
      bits_[begin].reset();
  }

private:
  std::vector<Bit> bits_;
};
