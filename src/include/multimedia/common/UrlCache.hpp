#pragma once

#include <string>
#include <list>
#include <algorithm>
#include <optional>
#include "multimedia/common/Singleton.hpp"

template <typename T>
class LruCache
{
public:
  static inline uint32_t kMaxCapacity = 10;

  void put(const T& value) {
    Mutex::lock locker(mutex_);
    if (auto it = std::find(data_.begin(), data_.end(), value); it != data_.end()) {
      data_.erase(it);
    }
    else if (data_.size() >= kMaxCapacity) {
      data_.pop_back();
    }

    data_.push_front(value);
  }
  std::optional<T> get(size_t index) {
    Mutex::lock locker(mutex_);
    if (index > data_.size()) {
      return {};
    }
    auto it = std::advance(data_.begin(), index);
    auto res = *it;
    data_.erase(it);
    data_.pop_front(res);
    return res;
  }

  std::list<T> getList() const { return data_; }

private:
  std::list<T> data_;
  Mutex::type mutex_;
};

using UrlCache = LruCache<std::string>;
using SingleUrlCache = Singleton<UrlCache>;
