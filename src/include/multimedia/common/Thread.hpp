#pragma once

#include <atomic>
#include <thread>
#include <future>
#include <unordered_map>

#include "noncopyable.hpp"

struct ThreadContext
{
  std::thread::id id{};
  std::string name{"undefined"};

  ThreadContext() {}
  ThreadContext(std::string_view threadName)
    : name(threadName)
  {}
  ThreadContext(std::thread::id threadId, std::string_view threadName)
    : id(threadId), name(threadName)
  {}

  void reset(std::thread::id threadId, std::string_view threadName)
  {
    id = threadId;
    name = threadName;
  }
  void reset(std::thread::id threadId)
  {
    id = threadId;
  }
  void clear()
  {
    id = std::thread::id{};
    name = "undefined";
  }
};

class Thread : public noncopyable
{
public:
  Thread() = default;
  Thread(const ThreadContext &context);
  Thread(std::string_view name);
  Thread(Thread&& th);
  ~Thread();

  template <typename Fn, typename... Args>
  std::future<std::invoke_result_t<Fn, Args...>>
    dispatch(Fn&& fn, Args&&... args)
  {
    using ResultType = std::invoke_result_t<Fn, Args...>;

    if (running_) this->stop();

    auto task = std::make_shared<std::packaged_task<ResultType(Args...)>>(std::forward<Fn>(fn));
    auto res = task->get_future();

    running_ = true;
    thread_ = std::thread([task, &args...]{(*task)(std::forward<Args>(args)...);});
    context_.reset(thread_.get_id());

    s_thread_mapping[thread_.get_id()] = context_;

    return res;
  }

  void start();
  void stop();

  virtual void run() {}

  ThreadContext context() const;

  static ThreadContext context(std::thread::id tid) {
    auto it = s_thread_mapping.find(tid);
    if (it != s_thread_mapping.end())
      return it->second;

    return {};
  }
  static std::string name(std::thread::id tid) {
    return context(tid).name;
  }
  static bool include(std::thread::id tid) { return s_thread_mapping.find(tid) != s_thread_mapping.end(); }

private:
  std::thread thread_;
  std::atomic_bool running_{false};

  ThreadContext context_;

  static inline std::unordered_map<std::thread::id, ThreadContext> s_thread_mapping = {};
};

