﻿#pragma once

#include <list>
#include <deque>

#include "multimedia/common/FFmpegUtil.hpp"

template <typename T>
class AVQueue
{
public:
  AVQueue() = default;
  virtual ~AVQueue() = default;

  void open() {
    running_ = true;
    cond_.notify_all();
  }
  void close() {
    running_ = false;
    cond_.notify_all();
  }

  bool push(const T& x) {
    {
      Mutex::ulock locker(cond_mutex_);
      cond_.wait(locker, [this] { 
        return data_.size() <= max_size_ / 5 || !running_; 
      });
      if (!running_) return false;
    }
    
    Mutex::lock locker1(mutex_);
    data_.push_back(x);
    return true;
  }
  bool push(T&& x) {
    {
      Mutex::ulock locker(cond_mutex_);
      cond_.wait(locker, [this] { 
        return data_.size() <= max_size_ / 5 || !running_; 
      });
      if (!running_) return false;
    }

    Mutex::lock locker1(mutex_);
    data_.push_back(x);
    return true;
  }
  bool pop(T& x) {
    if (isEmptyInternal()) {
      return false;
    }
    Mutex::lock locker(mutex_);
    if (isEmptyInternal()) {
      return false;
    }
    x = data_.front();
    data_.pop_front();
    cond_.notify_one();
    return true;
  }
  T peek() const {
    Mutex::lock locker(mutex_);
    if (isEmptyInternal()) {
      return {};
    }
    return data_.front();
  }
  T peekLatest() const {
    Mutex::lock locker(mutex_);
    if (isEmptyInternal()) {
      return {};
    }
    return data_.back();
  }

  void clear() {
    Mutex::lock locker(mutex_);
    data_.clear();
  }

  bool isEmpty() const {
    Mutex::lock locker(mutex_);
    return isEmptyInternal();
  }
  bool isFull() const {
    Mutex::lock locker(mutex_);
    return isFullInternal();
  }
  size_t getSize() const { return data_.size(); }
  size_t getMaxSize() const { return max_size_; }
  void setMaxSize(size_t maxSize) { max_size_ = maxSize; }

private:
  bool isEmptyInternal() const {
    return data_.empty();
  }
  bool isFullInternal() const {
    return data_.size() >= max_size_;
  }
private:
  std::atomic<size_t> max_size_ = INT64_MAX;
  //std::list<T> data_;
  std::deque<T> data_;
  std::condition_variable cond_;
  mutable Mutex::type cond_mutex_;
  mutable Mutex::type mutex_;
  std::atomic_bool running_{false};
};

using AVFrameQueue = AVQueue<AVFramePtr>;
using AVPacketQueue = AVQueue<AVPacketPtr>;