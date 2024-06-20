#pragma once

#include "multimedia/common/Mutex.hpp"

template <typename T>
class Singleton
{
public:
  static T* instance() {
    if (!inst_) {
      mutex_.lock();
      if (!inst_) {
        inst_ = new T();
      }
      mutex_.unlock();
    }
    return inst_;
  }

private:
  static inline T* inst_{nullptr};
  static Mutex::type mutex_;
};
