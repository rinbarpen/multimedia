#pragma once

#include <mutex>
#include <shared_mutex>

class Mutex
{
public:
  using type = std::mutex;
  using lock = std::lock_guard<type>;
  using ulock = std::unique_lock<type>;
  using slock = std::scoped_lock<type>;

};

class RWMutex
{
public:
  using type = std::shared_mutex;
  using rlock = std::shared_lock<type>;
  using wlock = std::unique_lock<type>;
  using slock = std::scoped_lock<type>;

};

class RecursiveMutex 
{
public:
  using type = std::recursive_mutex;
  using lock = std::lock_guard<type>;
  using ulock = std::unique_lock<type>;
  using slock = std::scoped_lock<type>;

};

class TimedMutex 
{
public:
  using type = std::timed_mutex;
  using lock = std::lock_guard<type>;
  using ulock = std::unique_lock<type>;
  using slock = std::scoped_lock<type>;

};

class RWTimedMutex {
public:
  using type = std::shared_timed_mutex;
  using rlock = std::shared_lock<type>;
  using wlock = std::unique_lock<type>;
  using slock = std::scoped_lock<type>;
};

class RecursiveTimedMutex {
public:
  using type = std::recursive_timed_mutex;
  using lock = std::lock_guard<type>;
  using ulock = std::unique_lock<type>;
  using slock = std::scoped_lock<type>;
};

