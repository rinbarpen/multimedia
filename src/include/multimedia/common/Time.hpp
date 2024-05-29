#pragma once

#include <chrono>
#include <thread>
#include <string>
#include <sstream>

class Time
{
public:
  using BaseClockType = std::chrono::steady_clock;
  using BaseTimePoint = std::chrono::time_point<BaseClockType>;
  using BaseTimeType = std::chrono::nanoseconds;

  static BaseTimePoint now() {
    return BaseClockType::now();
  }
  static void sleep(int64_t time) { 
    std::this_thread::sleep_for(BaseTimeType(time));
  }
  template <typename TimeType>
  static TimeType elapse(BaseTimePoint begin, BaseTimePoint end = now()) {
    return std::chrono::duration_cast<TimeType>(end - begin);
  }

  static std::string toString(BaseTimePoint tp) {
    auto n = tp.time_since_epoch().count();
    return std::to_string(n);
  }
  static std::string toFormatString(const char *fmt = "%Y-%m-%d %H:%M:%S") {
    char buf[256];
    auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm tm;
#if defined(__LINUX__)
    ::localtime_r(&t, &tm);
#elif defined(__WIN__)
    ::localtime_s(&tm, &t);
#endif
    std::strftime(buf, sizeof(buf), fmt, &tm);
    return std::string{buf};
  }
};

class Clock
{
public:
  static void start() { 
    last_tp_ = Time::now();
  }
  static int64_t elapse() { 
    auto curr_tp = Time::now();
    auto elapsed = Time::elapse<std::chrono::microseconds>(last_tp_, curr_tp).count();
    last_tp_ = curr_tp;
    return elapsed;
  }
  static void sleep(int64_t us) {
    std::this_thread::sleep_for(std::chrono::microseconds(us));
  }

private:
  static inline Time::BaseTimePoint last_tp_;
};