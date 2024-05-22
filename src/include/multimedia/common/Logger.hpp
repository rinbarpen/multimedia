#pragma once

#include <cstdarg>
#include <cstdint>
#include <functional>
#include <chrono>
#include <future>
#include <fstream>
#include <list>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "multimedia/common/Mutex.hpp"
#include "multimedia/common/Thread.hpp"

#include <fmt/core.h>
#include <yaml-cpp/yaml.h>

#define __LogEventGen(level, timestamp) \
  std::make_shared<LogEvent>(     \
    level, std::this_thread::get_id(), __FILE__, __LINE__, __FUNCTION__, timestamp)

#define __LogEventGen2(level)                                          \
  std::make_shared<LogEvent>(level, std::this_thread::get_id(), __FILE__, __LINE__, __FUNCTION__, \
    std::chrono::duration_cast<std::chrono::milliseconds>(             \
      std::chrono::system_clock::now() \
      .time_since_epoch())             \
      .count())

#define __LogEventWrapperGen(pLogger, level, timestamp) \
  std::make_shared<LogEventWrapper>(              \
    __LogEventGen(level, timestamp), pLogger)

#define __LogEventWrapperGen2(pLogger, level) \
  std::make_shared<LogEventWrapper>(__LogEventGen2(level), pLogger)

#define __LOG_STREAM(pLogger, level) \
  __LogEventWrapperGen2(pLogger, level)->getSS()

#define LOG_ROOT()       LogManager::instance()->getRoot()
#define GET_LOGGER(name) LogManager::instance()->getLogger(name)

#define ILOG_TRACE_FMT(pLogger, fmt, ...)                \
  __LogEventWrapperGen2(pLogger, LogLevel::LTRACE) \
    ->getEvent()                                         \
    ->format(fmt, ##__VA_ARGS__)
#define ILOG_DEBUG_FMT(pLogger, fmt, ...)                \
  __LogEventWrapperGen2(pLogger, LogLevel::LDEBUG) \
    ->getEvent()                                         \
    ->format(fmt, ##__VA_ARGS__)
#define ILOG_INFO_FMT(pLogger, fmt, ...)                \
  __LogEventWrapperGen2(pLogger, LogLevel::LINFO) \
    ->getEvent()                                        \
    ->format(fmt, ##__VA_ARGS__)
#define ILOG_CRITICAL_FMT(pLogger, fmt, ...)                \
  __LogEventWrapperGen2(pLogger, LogLevel::LCRITICAL) \
    ->getEvent()                                            \
    ->format(fmt, ##__VA_ARGS__)
#define ILOG_WARN_FMT(pLogger, fmt, ...)                \
  __LogEventWrapperGen2(pLogger, LogLevel::LWARN) \
    ->getEvent()                                        \
    ->format(fmt, ##__VA_ARGS__)
#define ILOG_ERROR_FMT(pLogger, fmt, ...)                \
  __LogEventWrapperGen2(pLogger, LogLevel::LERROR) \
    ->getEvent()                                         \
    ->format(fmt, ##__VA_ARGS__)
#define ILOG_FATAL_FMT(pLogger, fmt, ...)                \
  __LogEventWrapperGen2(pLogger, LogLevel::LFATAL) \
    ->getEvent()                                         \
    ->format(fmt, ##__VA_ARGS__)


#define ILOG_TRACE(pLogger)    __LOG_STREAM(pLogger, LogLevel::LTRACE)
#define ILOG_DEBUG(pLogger)    __LOG_STREAM(pLogger, LogLevel::LDEBUG)
#define ILOG_INFO(pLogger)     __LOG_STREAM(pLogger, LogLevel::LINFO)
#define ILOG_CRITICAL(pLogger) __LOG_STREAM(pLogger, LogLevel::LCRITICAL)
#define ILOG_WARN(pLogger)     __LOG_STREAM(pLogger, LogLevel::LWARN)
#define ILOG_ERROR(pLogger)    __LOG_STREAM(pLogger, LogLevel::LERROR)
#define ILOG_FATAL(pLogger)    __LOG_STREAM(pLogger, LogLevel::LFATAL)


#define LOG_TRACE_FMT(fmt, ...)                             \
  __LogEventWrapperGen2(LOG_ROOT(), LogLevel::LTRACE) \
    ->getEvent()                                            \
    ->format(fmt, ##__VA_ARGS__)
#define LOG_DEBUG_FMT(fmt, ...)                             \
  __LogEventWrapperGen2(LOG_ROOT(), LogLevel::LDEBUG) \
    ->getEvent()                                            \
    ->format(fmt, ##__VA_ARGS__)
#define LOG_INFO_FMT(fmt, ...)                             \
  __LogEventWrapperGen2(LOG_ROOT(), LogLevel::LINFO) \
    ->getEvent()                                           \
    ->format(fmt, ##__VA_ARGS__)
#define LOG_CRITICAL_FMT(fmt, ...)                             \
  __LogEventWrapperGen2(LOG_ROOT(), LogLevel::LCRITICAL) \
    ->getEvent()                                               \
    ->format(fmt, ##__VA_ARGS__)
#define LOG_WARN_FMT(fmt, ...)                             \
  __LogEventWrapperGen2(LOG_ROOT(), LogLevel::LWARN) \
    ->getEvent()                                           \
    ->format(fmt, ##__VA_ARGS__)
#define LOG_ERROR_FMT(fmt, ...)                             \
  __LogEventWrapperGen2(LOG_ROOT(), LogLevel::LERROR) \
    ->getEvent()                                            \
    ->format(fmt, ##__VA_ARGS__)
#define LOG_FATAL_FMT(fmt, ...)                             \
  __LogEventWrapperGen2(LOG_ROOT(), LogLevel::LFATAL) \
    ->getEvent()                                            \
    ->format(fmt, ##__VA_ARGS__)


#define LOG_TRACE()    __LOG_STREAM(LOG_ROOT(), LogLevel::LTRACE)
#define LOG_DEBUG()    __LOG_STREAM(LOG_ROOT(), LogLevel::LDEBUG)
#define LOG_INFO()     __LOG_STREAM(LOG_ROOT(), LogLevel::LINFO)
#define LOG_CRITICAL() __LOG_STREAM(LOG_ROOT(), LogLevel::LCRITICAL)
#define LOG_WARN()     __LOG_STREAM(LOG_ROOT(), LogLevel::LWARN)
#define LOG_ERROR()    __LOG_STREAM(LOG_ROOT(), LogLevel::LERROR)
#define LOG_FATAL()    __LOG_STREAM(LOG_ROOT(), LogLevel::LFATAL)

// 2023-03-01    root[DEBUG]    Logger.h:121    main    thanks for using lycore
inline constexpr const char *kDefaultFormatPattern =
  "$DATETIME{%Y-%m-%d %H:%M:%S}"
  "$CHAR:\t$THREAD_NAME$CHAR:[$THREAD_ID$CHAR:]"
  "$CHAR:\t$LOG_NAME$CHAR:[$LOG_LEVEL$CHAR:]"
  "$CHAR:\t$FILENAME$CHAR::$LINE"
  "$CHAR:\t$FUNCTION_NAME"
  "$CHAR: | $MESSAGE$CHAR:\n";
// 2023-03-01    Logger.h:121    [DEBUG]    main    thanks for using lycore
inline constexpr const char *kCommonFormatPattern =
  "$DATETIME{%Y-%m-%d %H:%M:%S}"
  "$CHAR:\t$FILENAME$CHAR::$LINE"
  "$CHAR:\t$CHAR:[$LOG_LEVEL$CHAR:]"
  "$CHAR:\t$FUNCTION_NAME"
  "$CHAR:\t$MESSAGE$CHAR:\n";
// 2023-03-01    [DEBUG]    main    thanks for using lycore
inline constexpr const char *kCommon2FormatPattern =
  "$DATETIME{%Y-%m-%d %H:%M:%S}"
  "$CHAR:\t$CHAR:[$LOG_LEVEL$CHAR:]"
  "$CHAR:\t$FUNCTION_NAME"
  "$CHAR:\t$MESSAGE$CHAR:\n";
// 2023-03-01    [DEBUG]    thanks for using lycore
inline constexpr const char *kBriefFormatPattern =
  "$DATETIME{%Y-%m-%d %H:%M:%S}"
  "$CHAR:\t$CHAR:[$LOG_LEVEL$CHAR:]"
  "$CHAR:\t$MESSAGE$CHAR:\n";

enum LogIniterFlag : uint8_t {
  CONSOLE = 0x01,
  SYNC_FILE = 0x02,
  ASYNC_FILE = 0x04,
};

class LogLevel
{
public:
  enum Level : uint8_t
  {
    LUNKNOWN = 0,
    LTRACE = 1,
    LDEBUG = 2,
    LINFO = 3,
    LWARN = 4,
    LERROR = 5,
    LCRITICAL = 6,
    LFATAL = 7,
    LCLOSE = 8,
    /* CUSTOM */
  };

  LogLevel(LogLevel::Level level = LUNKNOWN) : level_(level) {}

  Level level() const { return level_; }
  std::string toString() const {
    switch (level_) {
#define XX(x) \
    case LogLevel::Level::L##x: return #x;

      XX(TRACE)
      XX(DEBUG)
      XX(INFO)
      XX(CRITICAL)
      XX(WARN)
    case LogLevel::Level::LERROR: return "ERROR";
      XX(FATAL)
#undef XX

    case LogLevel::Level::LUNKNOWN:
    default: return "NONE";
    }
  }

  bool operator<(const LogLevel &rhs) const { return level_ < rhs.level_; }
  bool operator>(const LogLevel &rhs) const { return !(*this <= rhs); }
  bool operator<=(const LogLevel &rhs) const { return level_ <= rhs.level_; }
  bool operator>=(const LogLevel &rhs) const { return !(*this < rhs); }
  bool operator==(const LogLevel &rhs) const { return level_ == rhs.level_; }
  bool operator!=(const LogLevel &rhs) const { return !(*this == rhs); }

  static LogLevel fromString(const std::string &str) {
#define XX(x)              \
  if (str == #x) {         \
    return LogLevel(L##x); \
  }

    XX(TRACE)
    XX(DEBUG)
    XX(INFO)
    XX(CRITICAL)
    XX(WARN)
    if (str == "ERROR") {
      return LogLevel(LERROR);
    }
    XX(FATAL)
#undef XX

    return LogLevel(LUNKNOWN);
  }

private:
  Level level_;
};

struct LogColorConfig
{
  enum ColorType : int
  {
    END = 0,
    RED = 1,
    GREEN = 2,
    YELLOW = 3,
    BLUE = 4,
    PURPLE = 5,
    LIGHT_BLUE = 6,
    DEEP_RED = 7,
  };

  const char *const colors[8] = {
    "\033[0m",     // END
    "\033[31m",    // Red
    "\033[32m",    // Green
    "\033[33m",    // Yellow
    "\033[34m",    // Blue
    "\033[35m",    // Purple
    "\033[36m",    // Cyan
    "\033[31;2m",  // Deep Red
  };

  int LOG_END = END;
  int LOG_LEVEL_TRACE = LIGHT_BLUE;
  int LOG_LEVEL_DEBUG = BLUE;
  int LOG_LEVEL_INFO = GREEN;
  int LOG_LEVEL_CRITICAL = PURPLE;
  int LOG_LEVEL_WARN = YELLOW;
  int LOG_LEVEL_ERROR = RED;
  int LOG_LEVEL_FATAL = DEEP_RED;

  const char *getColor(int type) const { return colors[type]; }
};

class Logger;

class LogEvent
{
public:
  using ptr = std::shared_ptr<LogEvent>;

  LogEvent(LogLevel level, std::thread::id tid, const std::string &filename, int32_t line,
    const std::string &functionName, int64_t timestamp,
    LogColorConfig config = LogColorConfig());

  std::string getFilename() const { return filename_; }
  std::string getFunctionName() const { return function_name_; }
  int32_t getLine() const { return line_; }
  int64_t getTimestamp() const { return timestamp_; }
  std::string getContent() const { return ss_.str(); }
  // TODO: add thread id and process id
  std::thread::id getThreadId() const { return tid_; }
  std::string getThreadName() const { return Thread::name(tid_); }
  // int getProcessId() const {}

  LogLevel getLevel() const { return level_; }
  LogColorConfig getColorConfig() const { return color_config_; }
  std::stringstream &getSS() { return ss_; }

#ifdef FMT_VERSION
  // fmt-style
  template <typename... Args>
  void format(::fmt::string_view fmt, Args &&...args) {
    ss_ << ::fmt::format(fmt, std::forward<Args>(args)...);
  }
#else
  // c-style
  template <typename... Args>
  void format(::std::string_view fmt, Args &&...args) {
    char buf[256];
    snprintf(buf, 256, fmt.data(), std::forward<Args>(args)...);
    ss_ << std::string(buf);
  }
#endif
private:
  std::thread::id tid_;
  std::string filename_;
  std::string function_name_;
  int32_t line_ = 0;
  int64_t timestamp_ = 0;
  std::stringstream ss_;
  LogLevel level_;
  LogColorConfig color_config_;
};

struct LogFormatterItem
{
  using ptr = std::shared_ptr<LogFormatterItem>;

  LogFormatterItem() = default;
  virtual ~LogFormatterItem() = default;

  virtual void format(std::ostream &os, LogEvent::ptr pLogEvent,
    std::shared_ptr<Logger> pLogger) = 0;
};

class LogFormatter
{
protected:
  static constexpr char ID_TOKEN = '$';
  static constexpr int FORMAT_ID_LOC = 0;
  static constexpr int FORMAT_FN_ARG_LOC = 1;
  static constexpr int STATUS_CODE_LOC = 2;

  enum
  {
    PARSE_OK = 0,
    PARSE_ERROR = 1,
  };

  using PatArgsWrapper = std::tuple<std::string, std::string, int>;

public:
  using ptr = std::shared_ptr<LogFormatter>;

  LogFormatter(const std::string &pattern = kDefaultFormatPattern);

  std::string format(LogEvent::ptr pLogEvent, std::shared_ptr<Logger> pLogger);
  std::ostream &format(
    std::ostream &os, LogEvent::ptr pLogEvent, std::shared_ptr<Logger> pLogger);

  bool hasError() const { return has_error_; }
  std::string lastError() const { return error_; }
  std::string getPattern() const { return pattern_; }

  YAML::Node toYaml() const;

private:
  PatArgsWrapper parsePatToken(const std::string &patToken);

  void init();

private:
  std::string pattern_;
  std::vector<LogFormatterItem::ptr> items_;
  std::string error_;
  bool has_error_{false};
};

class LogAppender
{
public:
  using ptr = std::shared_ptr<LogAppender>;

  LogAppender() = default;
  virtual ~LogAppender() = default;

  virtual void log(LogEvent::ptr pEvent, std::shared_ptr<Logger> pLogger) = 0;

  void setFormatter(LogFormatter::ptr pFormatter);
  LogFormatter::ptr getFormatter() const;

  LogLevel getLevel() const { return level_; }
  void setLevel(LogLevel level) { level_ = level; }

  virtual YAML::Node toYaml() const = 0;

protected:
  LogLevel level_{LogLevel::LTRACE};
  LogFormatter::ptr formatter_;

  mutable Mutex::type mutex_;
};

class FileLogAppender : public LogAppender
{
public:
  using ptr = std::shared_ptr<FileLogAppender>;

  FileLogAppender(const std::string &filename);
  virtual ~FileLogAppender() override = default;

  virtual void log(
    LogEvent::ptr pLogEvent, std::shared_ptr<Logger> pLogger) override;
  bool reopen();
  YAML::Node toYaml() const override;

private:
  std::string getWholeFilename();

private:
  /* real filename: filename_ + "_" + current_day + "_" + cnt{02d} + ".log" */
  std::string filename_;
  std::ofstream file_stream_;
  uint64_t last_access_time_{0};
  uint64_t lines_{0};
  uint8_t cnt_{0};  // cnt_ incr when lines_ encounters max lines
  int today_;
};
class AsyncFileLogAppender : public LogAppender
{
public:
  using ptr = std::shared_ptr<AsyncFileLogAppender>;

  AsyncFileLogAppender(const std::string &filename);
  virtual ~AsyncFileLogAppender() override = default;

  virtual void log(
    LogEvent::ptr pLogEvent, std::shared_ptr<Logger> pLogger) override;

  YAML::Node toYaml() const override;

private:
  bool reopen(struct tm *tm);
  std::string getWholeFilename(struct tm *tm) const;
  void reopenNewFileIfShould(int64_t timestamp);

private:
  /* real filename: filename_ + "_" + current_day + "_" + cnt{02d} + ".log" */
  std::string filename_;
  std::ofstream file_stream_;
  uint64_t lines_{0};
  uint8_t cnt_{0};  // cnt_ incr when lines_ encounters max lines
  int today_{0};
};
class StdoutLogAppender : public LogAppender
{
public:
  using ptr = std::shared_ptr<StdoutLogAppender>;

  StdoutLogAppender() = default;
  virtual ~StdoutLogAppender() override = default;

  virtual void log(
    LogEvent::ptr pLogEvent, std::shared_ptr<Logger> pLogger) override;
  YAML::Node toYaml() const override;
};

class Logger : public std::enable_shared_from_this<Logger>
{
public:
  using ptr = std::shared_ptr<Logger>;

  Logger(const std::string &name = "root");
  Logger(const std::string &name, LogLevel level, const std::string &pattern = kDefaultFormatPattern, uint8_t flags = LogIniterFlag::CONSOLE, const std::string &filename = "undefined");
  ~Logger() = default;

  void log(LogEvent::ptr pLogEvent);

  void addAppender(LogAppender::ptr pAppender);
  void removeAppender(LogAppender::ptr pAppender);
  void clearAppenders();

  YAML::Node toYaml() const;


  LogLevel getLevel() const { return level_; }
  void setLevel(LogLevel level) { level_ = level; }
  const std::string &getName() const { return name_; }

  void setFormatter(LogFormatter::ptr pFormatter);
  void setFormatter(const std::string &pattern);
  LogFormatter::ptr getFormatter() const;

  Logger::ptr getParent() { return parent_; }
  void setParent(Logger::ptr pLogger) { parent_ = pLogger; }

private:
  std::string name_;
  LogLevel level_{LogLevel::Level::LDEBUG};
  std::list<LogAppender::ptr> appenders_;
  LogFormatter::ptr formatter_;
  Logger::ptr parent_;

  mutable Mutex::type mutex_;
};


class LogEventWrapper
{
public:
  using ptr = std::shared_ptr<LogEventWrapper>;

  LogEventWrapper(LogEvent::ptr pEvent, Logger::ptr pLogger);
  ~LogEventWrapper() { logger_->log(event_); }

  LogEvent::ptr getEvent() const { return event_; }
  Logger::ptr getLogger() const { return logger_; }
  std::stringstream &getSS() { return event_->getSS(); }

private:
  LogEvent::ptr event_;
  Logger::ptr logger_;
};

class LogManager
{
public:
  using ptr = std::shared_ptr<LogManager>;

  static LogManager *instance() {
    static LogManager *manager = new LogManager();
    return manager;
  }
  ~LogManager() = default;

  Logger::ptr getLogger(const std::string &name);
  Logger::ptr getLogger2(const std::string &name);
  bool putLogger(Logger::ptr pLogger);
  void insert(Logger::ptr pLogger);

  // TODO: For future do
  void init() {}
  Logger::ptr getRoot() const { return root_; }
  std::string toYamlString() const;
  void toYamlFile(std::string_view filename) const;

private:
  LogManager();

  Mutex::type mutex_;
  std::map<std::string, Logger::ptr> loggers_;
  Logger::ptr root_;
};

class LogIniter
{
public:
  /* the appender's formatter is the same as the logger */
  static Logger::ptr reg(
    /* logger */
    const std::string &log_name, LogLevel log_level,
    /* formatter */
    const std::string &format_pattern = kDefaultFormatPattern,
    /* appender */
    bool write2file = true, const std::string &filename = "x",
    bool async = false);

  static Logger::ptr reg(const std::string &name, const std::string &split = ".", uint8_t flags = LogIniterFlag::CONSOLE, LogLevel level = LogLevel::LDEBUG, const std::string &pattern = kDefaultFormatPattern);

  static void loadYamlFile(const std::string& filename);
  static void loadYamlNode(YAML::Node node);
};

static std::string kLogBasePath = "./logs/";

/**
 * LogIniter::getLogger("sample",
 *                      "LogLevel::Level::LDEBUG",
 *                      kDefaultFormatPattern,
 *                      true,
 *                      "sample")
 *
 * sample_${DATE}_${COUNT}.log
 *
 */

inline void log_export_config() {
  LogManager::instance()->toYamlFile("config/log.yml");
}

inline void log_load_config() {
  LogIniter::loadYamlFile("config/log.yml");
}

static Logger::ptr base_logger(Logger::ptr pLogger) {
  Logger::ptr cur = pLogger;
  Logger::ptr pa;
  while (cur != pa) {
    pa = cur->getParent();
    if (pa->getName() == "root") return cur;

    cur = pa;
  }

  // cur == pa, the root logger isn't 'root'
  return cur;
}

static Logger::ptr GET_LOGGER2(const std::string &name, const std::string &parent = "root", uint8_t flags = LogIniterFlag::CONSOLE, LogLevel level = LogLevel::LDEBUG, const std::string &pattern = kDefaultFormatPattern) {
  auto me = LogManager::instance()->getLogger2(name);
  if (me) { return me; }

  auto pa = LogManager::instance()->getLogger2(parent);
  if (nullptr == pa) {
    return nullptr;
  }

  if ((flags & CONSOLE) != CONSOLE
   && (flags & SYNC_FILE) != SYNC_FILE
   && (flags & ASYNC_FILE) != ASYNC_FILE) {
    // invalid_argument
    // throw std::invalid_argument("No such flags");
    return nullptr;
  }

  auto pLogger = std::make_shared<Logger>(name);
  pLogger->setLevel(level);
  pLogger->setFormatter(pattern);

  auto filename = base_logger(pa)->getName();
  if ((flags & CONSOLE) == CONSOLE) {
    pLogger->addAppender(std::make_shared<StdoutLogAppender>());
  }
  if ((flags & SYNC_FILE) == SYNC_FILE) {
    pLogger->addAppender(std::make_shared<FileLogAppender>(filename));
  }
  else if ((flags & ASYNC_FILE) == ASYNC_FILE) {
    pLogger->addAppender(std::make_shared<AsyncFileLogAppender>(filename));
  }
  pLogger->setParent(pa);

  LogManager::instance()->insert(pLogger);
  return pLogger;
}

// Dot
// Examples: input system.debugger.console -> create system, system.debugger, system.debugger.console logger in sequences
static Logger::ptr GET_LOGGER3(const std::string &name, const std::string &split = ".", uint8_t flags = LogIniterFlag::CONSOLE, LogLevel level = LogLevel::LDEBUG, const std::string &pattern = kDefaultFormatPattern) {
  auto me = LogManager::instance()->getLogger2(name);
  if (me) { return me; }

  if ((flags & CONSOLE) != CONSOLE
   && (flags & SYNC_FILE) != SYNC_FILE
   && (flags & ASYNC_FILE) != ASYNC_FILE) {
    // invalid_argument
    // throw std::invalid_argument("No such flags");
    return nullptr;
  }

  auto firstDot = name.find(split);
  auto lastDot = name.rfind(split);
  auto pLogger = std::make_shared<Logger>(name);
  pLogger->setLevel(level);
  pLogger->setFormatter(pattern);

  auto filename = name.substr(0, firstDot);  // if no dot, this is myself
  if ((flags & CONSOLE) == CONSOLE) {
    pLogger->addAppender(std::make_shared<StdoutLogAppender>());
  }
  if ((flags & SYNC_FILE) == SYNC_FILE) {
    pLogger->addAppender(std::make_shared<FileLogAppender>(filename));
  }
  else if ((flags & ASYNC_FILE) == ASYNC_FILE) {
    pLogger->addAppender(std::make_shared<AsyncFileLogAppender>(filename));
  }

  if (firstDot != std::string::npos) {
    pLogger->setParent(GET_LOGGER3(name.substr(0, lastDot), split, flags, level, pattern));
  }

  LogManager::instance()->insert(pLogger);
  return pLogger;
}
