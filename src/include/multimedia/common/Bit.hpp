#pragma once

#include <utility>

class Bit 
{
public:
  Bit() = default;
  Bit(bool value) : value_(value) {}
  ~Bit() = default;

  void set() { value_ = true; }
  void unset() { value_ = false; }
  bool get() const { return value_; } 

  operator bool() { return value_; }
  operator bool() const { return value_; }

  template <typename Fn, typename... Args>
  void doSomething(Fn &&fn, Args&&... args) {
    if (value_) {
      std::forward<Fn>(fn)(std::forward<Args>(args)...);
    }
  }
  template <typename Fn, typename... Args>
  void doSomethingIfNot(Fn &&fn, Args &&...args) {
    if (!value_) {
      std::forward<Fn>(fn)(std::forward<Args>(args)...);
    }
  }
  template <typename Fn, typename... Args>
  void doLoop(Fn &&fn, Args &&...args) {
    while (value_) {
      std::forward<Fn>(fn)(std::forward<Args>(args)...);
    }
  }
  template <typename Fn, typename... Args>
  void doLoopIfNot(Fn &&fn, Args &&...args) {
    while (!value_) {
      std::forward<Fn>(fn)(std::forward<Args>(args)...);
    }
  }

private:
  bool value_{false};
};
