#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <string>
#include <thread>

#include "multimedia/FFmpegPlayer.hpp"
#include "multimedia/common/Mutex.hpp"
#include "multimedia/common/noncopyable.hpp"

class FFmpegPlayer;
class AVThread : public noncopyable
{
public:
  AVThread() = default;
  ~AVThread() {
    if (thread_.joinable()) {
      thread_.join();
    }
  }

  template <typename Fn, typename... Args>
  void dispatch(Fn &&fn, Args&&... args) {
    thread_ = std::thread(std::forward<Fn>(fn), std::forward<Args>(args)...);
  }
  void join() { thread_.join(); }
  bool isJoinable() const { return thread_.joinable(); }
  void detach() { thread_.detach(); }

private:
  std::thread thread_;
};

class AVReadThread : public AVThread
{
public:
  void start(FFmpegPlayer* player) {

  }

  void stop() {
    join();
  }
private:
  std::condition_variable cond_;
  Mutex::type mutex_;
};

class AVAudioDecodeThread : public AVThread
{
public:

  void start(FFmpegPlayer* player) {

  }
  void stop() {
    join();
  }
};

class AVVideoDecodeThread : public AVThread
{
public:
  void start(FFmpegPlayer* player) {

  }
  void stop() {
    join();
  }

};