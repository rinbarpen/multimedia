#pragma once

#include <condition_variable>

#include "multimedia/common/Mutex.hpp"

class ConditionVariable
{
public:
  template <typename Fn>
  void wait(Fn &&fn) {
    Mutex::ulock locker(mutex_);
    cond_.wait(locker, fn);
  }
  template <typename Time, typename Fn>
  void waitFor(const Time &time, Fn &&fn) {
    Mutex::ulock locker(mutex_);
    cond_.wait_for(locker, time, fn);
  }

  void signal() {
    cond_.notify_one();
  }
  void signalAll() {
    cond_.notify_all();
  }
private:
  std::condition_variable cond_;
  Mutex::type mutex_;
};