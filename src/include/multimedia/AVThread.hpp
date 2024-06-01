#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <string>
#include <thread>

#include "multimedia/common/Mutex.hpp"
#include "multimedia/common/noncopyable.hpp"

class FFmpegPlayer;
class AVThread : public noncopyable
{
public:
  AVThread() = default;
  ~AVThread() { thread_.join(); }

  template <typename Fn, typename... Args>
  void dispatch(Fn &&fn, Args &&...args) {
    thread_ = std::thread(std::forward<Fn>(fn), std::forward<Args>(args)...);
  }
  void stop() {
    if (thread_.joinable()) thread_.join();
  }
  void join() {
    thread_.join();
  }
  bool isJoinable() const { return thread_.joinable(); }
  void detach() { thread_.detach(); }

private:
  std::thread thread_;
};
