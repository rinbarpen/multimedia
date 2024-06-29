#include <iostream>
#include <string>

#include <multimedia/common/OSUtil.hpp>
#include <multimedia/common/Logger.hpp>

static auto g_max_file_line = 50000;

/********************************************* LogEvent
 * **********************************************/
LogEvent::LogEvent(LogLevel level, std::thread::id tid,
  const std::string &filename, int32_t line, const std::string &function_name,
  int64_t timestamp, LogColorConfig config)
  : tid_(tid)
  , filename_(filename)
  , function_name_(function_name)
  , line_(line)
  , timestamp_(timestamp)
  , level_(level) {}

/***************************************** LogFormatterItem
 * ******************************************/
class MessageFormatterItem final : public LogFormatterItem
{
public:
  MessageFormatterItem(const std::string &str = "") {}
  void format(std::ostream &os, LogEvent::ptr pLogEvent,
    std::shared_ptr<Logger> pLogger) override {
    os << pLogEvent->getContent();
  }
};
class LogLevelFormatterItem final : public LogFormatterItem
{
public:
  LogLevelFormatterItem(const std::string &str = "") {}
  void format(std::ostream &os, LogEvent::ptr pLogEvent,
    std::shared_ptr<Logger> pLogger) override {
    os << pLogEvent->getLevel().toString();
  }
};
class LogNameFormatterItem final : public LogFormatterItem
{
public:
  LogNameFormatterItem(const std::string &str = "") {}
  void format(std::ostream &os, LogEvent::ptr pLogEvent,
    std::shared_ptr<Logger> pLogger) override {
    os << pLogger->getName();
  }
};
class DateTimeFormatterItem final : public LogFormatterItem
{
public:
  DateTimeFormatterItem(const std::string &format = "%Y-%m-%d %H:%M:%S")
    : timefmt_(format) {
    if (timefmt_.empty()) {
      timefmt_ = "%Y-%m-%d %H:%M:%S";
    }
  }
  void format(std::ostream &os, LogEvent::ptr pLogEvent,
    std::shared_ptr<Logger> pLogger) override {
    const auto timestamp = pLogEvent->getTimestamp();
    const auto tp = std::chrono::time_point<std::chrono::system_clock>(
      std::chrono::milliseconds(timestamp));
    time_t t = std::chrono::system_clock::to_time_t(tp);
#ifdef __WIN__
    struct tm tm;
    localtime_s(&tm, &t);
#elif defined(__LINUX__)
    struct tm tm;
    localtime_r(&t, &tm);
#else
    struct tm tm = *localtime(&t);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), timefmt_.c_str(), &tm);
    os << buf;
  }

private:
  std::string timefmt_;
};
class FilenameFormatterItem final : public LogFormatterItem
{
public:
  FilenameFormatterItem(const std::string &str = "") {}
  void format(std::ostream &os, LogEvent::ptr pLogEvent,
    std::shared_ptr<Logger> pLogger) override {
    auto name = pLogEvent->getFilename();
    std::string basename;
    auto p = name.rfind('/');
    if (p != std::string::npos) {
      basename = name.substr(p + 1);
    }
    else {
      basename = name;
    }
    os << basename;
  }
};
class LineFormatterItem final : public LogFormatterItem
{
public:
  LineFormatterItem(const std::string &str = "") {}
  void format(std::ostream &os, LogEvent::ptr pLogEvent,
    std::shared_ptr<Logger> pLogger) override {
    os << pLogEvent->getLine();
  }
};
class StringFormatterItem final : public LogFormatterItem
{
public:
  StringFormatterItem(const std::string &str) : str_(str) {}
  void format(std::ostream &os, LogEvent::ptr pLogEvent,
    std::shared_ptr<Logger> pLogger) override {
    os << str_;
  }

private:
  std::string str_;
};
class CharFormatterItem final : public LogFormatterItem
{
public:
  CharFormatterItem(const std::string &str = "") : str_(str) {}
  void format(std::ostream &os, LogEvent::ptr pLogEvent,
    std::shared_ptr<Logger> pLogger) override {
    os << str_;
  }

private:
  std::string str_;
};
class FunctionNameFormatterItem final : public LogFormatterItem
{
public:
  FunctionNameFormatterItem(const std::string &str) : str_(str) {}
  void format(std::ostream &os, LogEvent::ptr pLogEvent,
    std::shared_ptr<Logger> pLogger) override {
    os << pLogEvent->getFunctionName();
  }

private:
  std::string str_;
};
class ThreadNameFormatterItem : public LogFormatterItem
{
public:
  ThreadNameFormatterItem(const std::string &str) : str_(str) {}
  void format(std::ostream &os, LogEvent::ptr pLogEvent,
    std::shared_ptr<Logger> pLogger) override {
    os << pLogEvent->getThreadName();
  }

private:
  std::string str_;
};
class ThreadIdFormatterItem : public LogFormatterItem
{
public:
  ThreadIdFormatterItem(const std::string &str) : str_(str) {}
  void format(std::ostream &os, LogEvent::ptr pLogEvent,
    std::shared_ptr<Logger> pLogger) override {
    os << pLogEvent->getThreadId();
  }

private:
  std::string str_;
};


/******************************************* LogFormatter
 * *******************************************/
LogFormatter::LogFormatter(const std::string &pattern) : pattern_(pattern) {
  init();
}

std::string LogFormatter::format(
  LogEvent::ptr pLogEvent, std::shared_ptr<Logger> pLogger) {
  std::stringstream ss;
  for (auto &item : items_) {
    item->format(ss, pLogEvent, pLogger);
  }
  return ss.str();
}

std::ostream &LogFormatter::format(
  std::ostream &os, LogEvent::ptr pLogEvent, std::shared_ptr<Logger> pLogger) {
  std::stringstream ss;
  for (auto &item : items_) {
    item->format(ss, pLogEvent, pLogger);
  }
  os << ss.str();
  os.flush();
  return os;
}

LogFormatter::PatArgsWrapper LogFormatter::parsePatToken(
  const std::string &patToken) {
  if (patToken.find("CHAR:") == 0) {
    if (patToken.length() <= 5) return std::make_tuple("CHAR", "", PARSE_ERROR);
    auto ch = patToken.substr(5);
    return std::make_tuple("CHAR", ch, PARSE_OK);
  }
  if (patToken.find("DATETIME") == 0) {
    if (patToken.length() > 8 && patToken[8] == '{') {
      size_t timefmt_len = patToken.rfind('}');
      if (timefmt_len >= 9) {
        timefmt_len -= 9;
        auto timefmt = patToken.substr(9, timefmt_len);
        return std::make_tuple("DATETIME", timefmt, PARSE_OK);
      }
      else {
        // error timefmt
        return std::make_tuple(patToken, "", PARSE_ERROR);
      }
    }
    else {
      // Default DATETIME format
      return std::make_tuple("DATETIME", "%Y-%m-%d %H:%M:%S", PARSE_OK);
    }
  }
  // NO PARAM ARG
  return {patToken, "", PARSE_OK};
}

void LogFormatter::init() {
  std::vector<PatArgsWrapper> vec;
  std::string nstr;
  size_t start_pos = 0, len = 0;
  for (size_t i = 0; i < pattern_.size(); ++i) {
    if (pattern_[i] == ID_TOKEN) {
      if (len != 0) {
        nstr = pattern_.substr(start_pos, len);
        vec.push_back(parsePatToken(nstr));
      }

      start_pos = i + 1;
      len = 0;
      continue;
    }

    ++len;
  }

  if (len != 0) {
    nstr = pattern_.substr(start_pos, len);
    vec.push_back(parsePatToken(nstr));
  }
  else {
    // $
    vec.push_back(std::make_tuple("", "", PARSE_ERROR));
  }

  static std::unordered_map<std::string,
    std::function<LogFormatterItem::ptr(const std::string &str)>>
    s_format_items = {
#define XX(STR, ID)                                            \
  {                                                            \
    STR, [](const std::string &str) -> LogFormatterItem::ptr { \
      return std::make_shared<ID>(str);                        \
    }                                                          \
  }
      XX("LOG_LEVEL", LogLevelFormatterItem),
      XX("MESSAGE", MessageFormatterItem),
      XX("LOG_NAME", LogNameFormatterItem),
      XX("DATETIME", DateTimeFormatterItem),
      XX("FILENAME", FilenameFormatterItem),
      XX("LINE", LineFormatterItem),
      XX("CHAR", CharFormatterItem),
      XX("FUNCTION_NAME", FunctionNameFormatterItem),
      XX("THREAD_NAME", ThreadNameFormatterItem),
      XX("THREAD_ID", ThreadIdFormatterItem),
#undef XX
    };

  has_error_ = false;
  for (const auto &wrapper : vec) {
    const auto &[id, arg, status] = wrapper;
    if (status != PARSE_OK) {
      items_.push_back(std::make_shared<StringFormatterItem>(id));
      continue;
    }

    auto it = s_format_items.find(id);
    if (it == s_format_items.end()) {
      has_error_ = true;
      error_.clear();
      error_.append("<<PATTERN ERROR: UNSUPPORTED FORMAT $");
      error_.append(id);
      error_.append(">>");
      items_.push_back(std::make_shared<StringFormatterItem>(error_));
    }
    else {
      items_.push_back(it->second(arg));
    }
  }
}

YAML::Node LogFormatter::toYaml() const {
  YAML::Node node;
  node["pattern"] = pattern_;
  return node;
}


/******************************************* LogAppender
 * *********************************************/
void LogAppender::setFormatter(LogFormatter::ptr pFormatter) {
  Mutex::lock locker(mutex_);
  formatter_ = pFormatter;
}

LogFormatter::ptr LogAppender::getFormatter() const {
  Mutex::lock locker(mutex_);
  return formatter_;
}

/***************************************** FileLogAppender
 * *******************************************/
FileLogAppender::FileLogAppender(const std::string &filename)
  : filename_(kLogBasePath + filename) {
  reopen();
}

void FileLogAppender::log(
  LogEvent::ptr pLogEvent, std::shared_ptr<Logger> pLogger) {
  if (pLogEvent->getLevel() >= level_) {
    uint64_t now = pLogEvent->getTimestamp();
    if (now >= (last_access_time_ + 3)) {
      reopen();
      last_access_time_ = now;
    }

    Mutex::lock locker(mutex_);
    if (!formatter_->format(file_stream_, pLogEvent, pLogger)) {
      std::cerr << "error in "
                << "FileLogAppender::log "
                << "with Formatter format" << std::endl;
      std::cerr << "log file cannot be created" << std::endl;
    }
    else {
      ++lines_;
      if (lines_ >= g_max_file_line) {
        ++cnt_;
        lines_ = 0;
      }
    }
  }
}

bool FileLogAppender::reopen() {
  Mutex::lock locker(mutex_);
  if (file_stream_) {
    file_stream_.close();
  }

  // file_stream_.open(filename_, std::ios::app);
  os_api::touch(getWholeFilename());
  file_stream_.open(getWholeFilename(), std::ios::app);
  return file_stream_.is_open();
}

YAML::Node FileLogAppender::toYaml() const {
  YAML::Node node;
  node["type"] = "SyncFileLogAppender";
  node["filename"] = filename_;
  node["lines"] = lines_;
  node["count"] = static_cast<uint32_t>(cnt_);
  node["today"] = today_;

  node["level"] = level_.toString();
  if (formatter_) {
    node["formatter"] = formatter_->toYaml();
  }

  return node;
}

std::string FileLogAppender::getWholeFilename() {
  std::string wholeFilename;
  time_t t =
    std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
#ifdef __WIN__
  struct tm tm;
  localtime_s(&tm, &t);
#elif defined(__LINUX__)
  struct tm tm;
  localtime_r(&t, &tm);
#else
  struct tm tm = *localtime(&t);
#endif

  char todayStr[30];
  std::strftime(todayStr, 20, "%Y-%m-%d", &tm);
  const int today = tm.tm_yday;
  if (today != today_) {
    today_ = today;
    cnt_ = 0;
  }
  wholeFilename.append(filename_);
  wholeFilename.append("_");
  wholeFilename.append(todayStr);
  wholeFilename.append("_");
  if (cnt_ < 10) wholeFilename.append("0");
  wholeFilename.append(std::to_string(cnt_));
  wholeFilename.append(".log");

  return wholeFilename;
}

/*************************************** AsyncFileLogAppender
 * ***************************************/
AsyncFileLogAppender::AsyncFileLogAppender(const std::string &filename)
  : filename_(kLogBasePath + filename) {}
void AsyncFileLogAppender::log(
  LogEvent::ptr pLogEvent, std::shared_ptr<Logger> pLogger) {
  if (pLogEvent->getLevel() >= level_) {
    (void) std::async(std::launch::async, [&]() {
      Mutex::lock locker(mutex_);

      int64_t now = pLogEvent->getTimestamp();
      reopenNewFileIfShould(now);
      if (!formatter_->format(file_stream_, pLogEvent, pLogger)) {
        std::cerr << "error in "
                  << "AsyncFileLogAppender::log"
                  << " with Formatter format" << std::endl;
        std::cerr << "log file cannot be created" << std::endl;
      }
      else {
        lines_++;
      }
    });
  }
}

YAML::Node AsyncFileLogAppender::toYaml() const {
  YAML::Node node;
  node["type"] = "AsyncFileLogAppender";
  node["filename"] = filename_;
  node["lines"] = lines_;
  node["count"] = static_cast<uint32_t>(cnt_);
  node["today"] = today_;

  node["level"] = level_.toString();
  if (formatter_) {
    node["formatter"] = formatter_->toYaml();
  }

  return node;
}

bool AsyncFileLogAppender::reopen(tm *tm) {
  if (file_stream_) {
    file_stream_.close();
  }

  os_api::touch(getWholeFilename(tm));
  file_stream_.open(getWholeFilename(tm), std::ios::app);
  return file_stream_.is_open();
}

std::string AsyncFileLogAppender::getWholeFilename(tm *tm) const {
  std::string wholeFilename;

  char todayStr[20];
  std::strftime(todayStr, 20, "%Y-%m-%d", tm);
  wholeFilename.append(filename_);
  wholeFilename.append("_");
  wholeFilename.append(todayStr);
  wholeFilename.append("_");
  if (cnt_ < 10) wholeFilename.append("0");
  wholeFilename.append(std::to_string(cnt_));
  wholeFilename.append(".log");

  return wholeFilename;
}

void AsyncFileLogAppender::reopenNewFileIfShould(int64_t timestamp) {
  time_t t = timestamp / 1000;
#ifdef __WIN__
  struct tm tm;
  localtime_s(&tm, &t);
#elif defined(__LINUX__)
  struct tm tm;
  localtime_r(&t, &tm);
#else
  struct tm tm = *localtime(&t);
#endif

  const int today = tm.tm_yday;
  if (today != today_) {
    today_ = today;
    // switch to new file
    cnt_ = 0;
    lines_ = 0;
    reopen(&tm);
    return;
  }

  if (lines_ >= g_max_file_line) {
    // switch to new file
    cnt_++;
    lines_ = 0;
    reopen(&tm);
    return;
  }

  // donothing
}

/**************************************** StdoutLogAppender
 * *****************************************/
void StdoutLogAppender::log(
  LogEvent::ptr pLogEvent, std::shared_ptr<Logger> pLogger) {
  if (pLogEvent->getLevel() >= level_) {
    Mutex::lock locker(mutex_);

    LogColorConfig conf = pLogEvent->getColorConfig();
    switch (pLogEvent->getLevel().level()) {
#define XX(x) \
  case LogLevel::L##x: std::cout << conf.getColor(conf.LOG_LEVEL_##x); break;

      XX(TRACE)
      XX(DEBUG)
      XX(INFO)
      XX(CRITICAL)
      XX(WARN)
      XX(ERROR)
      XX(FATAL)
#undef XX
    default:;
    }

    formatter_->format(std::cout, pLogEvent, pLogger);
    if (pLogEvent->getLevel() > LogLevel::LUNKNOWN)
      std::cout << conf.getColor(conf.LOG_END);
  }
}

YAML::Node StdoutLogAppender::toYaml() const {
  YAML::Node node;
  node["type"] = "StdoutLogAppender";

  node["level"] = level_.toString();
  if (formatter_) {
    node["formatter"] = formatter_->toYaml();
  }

  return node;
}

/********************************************** Logger
 * **********************************************/
Logger::Logger(const std::string &name)
  : name_(name), formatter_(new LogFormatter()) {}
Logger::Logger(const std::string &name, LogLevel level, const std::string &pattern, uint8_t flags, const std::string &filename)
  : name_(name), level_(level), formatter_(new LogFormatter(pattern)) {
  if ((flags & LogIniterFlag::CONSOLE) == LogIniterFlag::CONSOLE) {
    auto pAppender = std::make_shared<StdoutLogAppender>();
    pAppender->setFormatter(formatter_);
    appenders_.push_back(pAppender);
  }
  if ((flags & LogIniterFlag::ASYNC_FILE) == LogIniterFlag::ASYNC_FILE) {
    auto pAppender = std::make_shared<AsyncFileLogAppender>(filename);
    pAppender->setFormatter(formatter_);
    appenders_.push_back(pAppender);
  }
  else if ((flags & LogIniterFlag::SYNC_FILE) == LogIniterFlag::SYNC_FILE) {
    auto pAppender = std::make_shared<FileLogAppender>(filename);
    pAppender->setFormatter(formatter_);
    appenders_.push_back(pAppender);
  }
}

void Logger::log(LogEvent::ptr pLogEvent) {
  if (pLogEvent->getLevel() >= level_) {
    auto self = shared_from_this();
    Mutex::lock locker(mutex_);
    if (!appenders_.empty()) {
      for (auto &pAppender : appenders_) {
        pAppender->log(pLogEvent, self);
      }
    }
    else if (parent_)
      parent_->log(pLogEvent);
  }
}

void Logger::addAppender(LogAppender::ptr pAppender) {
  Mutex::lock locker(mutex_);

  if (!pAppender->getFormatter()) {
    pAppender->setFormatter(formatter_);
  }
  appenders_.push_back(pAppender);
}
void Logger::removeAppender(LogAppender::ptr pAppender) {
  Mutex::lock locker(mutex_);
  auto it = std::find(appenders_.begin(), appenders_.end(), pAppender);
  if (it != appenders_.end()) {
    appenders_.erase(it);
  }
}
void Logger::clearAppenders() {
  Mutex::lock locker(mutex_);
  appenders_.clear();
}

void Logger::setFormatter(LogFormatter::ptr pFormatter) {
  Mutex::lock locker(mutex_);
  formatter_ = pFormatter;
}
void Logger::setFormatter(const std::string &pattern) {
  Mutex::lock locker(mutex_);
  formatter_ = std::make_shared<LogFormatter>(pattern);
}
LogFormatter::ptr Logger::getFormatter() const {
  Mutex::lock locker(mutex_);
  return formatter_;
}

YAML::Node Logger::toYaml() const {
  YAML::Node node;

  node["name"] = this->name_;
  node["level"] = this->level_.toString();
  if (!appenders_.empty()) {
    for (const auto &appender : appenders_) {
      node["appenders"].push_back(appender->toYaml());
    }
  }
  node["formatter"] = formatter_->toYaml();
  node["parent"] = parent_ ? parent_->getName() : "root";

  return node;
}

/***************************************** LogEventWrapper
 * ******************************************/

LogEventWrapper::LogEventWrapper(LogEvent::ptr pEvent, Logger::ptr pLogger)
  : event_(pEvent), logger_(pLogger) {}

/******************************************** LogManager
 * ********************************************/
LogManager::LogManager() {
  root_.reset(new Logger());
  auto pAppender = std::make_shared<StdoutLogAppender>();
  // pAppender->setFormatter(root_->getFormatter());
  root_->addAppender(pAppender);

  loggers_[root_->getName()] = root_;

  init();
}

Logger::ptr LogManager::getLogger(const std::string &name) {
  Mutex::lock locker(mutex_);
  auto it = loggers_.find(name);
  if (it != loggers_.end()) {
    return it->second;
  }

  auto pLogger = std::make_shared<Logger>(name, LogLevel::LDEBUG);
  pLogger->setParent(root_);
  loggers_[name] = pLogger;
  return pLogger;
}
Logger::ptr LogManager::getLogger2(const std::string &name) {
  Mutex::lock locker(mutex_);
  auto it = loggers_.find(name);
  if (it != loggers_.end()) {
    return it->second;
  }
  return nullptr;
}
bool LogManager::putLogger(Logger::ptr pLogger) {
  if (auto it = loggers_.find(pLogger->getName()); it != loggers_.end()) {
    return false;
  }
  loggers_[pLogger->getName()] = pLogger;
  return true;
}
void LogManager::insert(Logger::ptr pLogger) {
  loggers_[pLogger->getName()] = pLogger;
}

std::string LogManager::toYamlString() const {
  YAML::Node node;

  for (auto &[k, v] : loggers_) {
    node["logger"].push_back(v->toYaml());
  }

  return (std::ostringstream{} << node).str();
}

void LogManager::toYamlFile(std::string_view filename) const {
  os_api::rm(std::string{filename});

  YAML::Node node;
  for (auto &[k, v] : loggers_) {
    node["logger"].push_back(v->toYaml());
  }
  std::ofstream{filename.data()} << node;
}

/******************************************** LogIniter
 * *********************************************/
Logger::ptr LogIniter::reg(const std::string &log_name, LogLevel log_level,
  const std::string &format_pattern, bool write2file,
  const std::string &filename, bool async) {
  auto pLogger = LogManager::instance()->getLogger(log_name);
  pLogger->setLevel(log_level);
  if (format_pattern != kDefaultFormatPattern)
    pLogger->setFormatter(format_pattern);

  if (write2file) {
    if (async)
      pLogger->addAppender(std::make_shared<AsyncFileLogAppender>(filename));
    else
      pLogger->addAppender(std::make_shared<FileLogAppender>(filename));
  }
  else {
    pLogger->addAppender(std::make_shared<StdoutLogAppender>());
  }

  return pLogger;
}

Logger::ptr LogIniter::reg(const std::string &name, const std::string &split,
  uint8_t flags, LogLevel level, const std::string &pattern) {
  if ((flags & CONSOLE) != CONSOLE && (flags & SYNC_FILE) != SYNC_FILE
      && (flags & ASYNC_FILE) != ASYNC_FILE) {
    // invalid_argument
    // throw std::invalid_argument("No such flags");
    return nullptr;
  }

  auto firstDot = name.find(split);
  auto lastDot = name.rfind(split);
  auto pLogger = LogManager::instance()->getLogger2(name);
  if (pLogger) {
    pLogger->clearAppenders();
  }
  else {
    pLogger = std::make_shared<Logger>(name);
  }
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
    pLogger->setParent(
      reg(name.substr(0, lastDot), split, flags, level, pattern));
  }

  LogManager::instance()->insert(pLogger);
  return pLogger;
}

void LogIniter::loadYamlFile(const std::string &filename) {
  auto node = YAML::LoadFile(filename);
  if (!node["logger"].IsDefined()) return;

  (void) os_api::mkdir(kLogBasePath);

  loadYamlNode(node);
}

void LogIniter::loadYamlNode(YAML::Node node) {
  auto compare = [](const YAML::Node &lhs, const YAML::Node &rhs) {
    return lhs["name"].as<std::string>() < lhs["name"].as<std::string>();
  };

  std::set<YAML::Node, decltype(compare)> loggers(
    node["logger"].begin(), node["logger"].end(), compare);

  for (auto it = loggers.begin(); it != loggers.end(); ++it) {
    YAML::Node cur = *it;
    // parse logger
    auto pLogger = std::make_shared<Logger>(cur["name"].as<std::string>(),
      LogLevel::fromString(cur["level"].as<std::string>()),
      cur["formatter"]["pattern"].as<std::string>());

    if (!cur["appenders"].IsDefined() || node.IsNull()) {
      continue;
    }

    for (auto appender : node["appenders"]) {
      auto app_node = YAML::Load(appender.as<std::string>());
      auto &&app_type = app_node["type"].as<std::string>();
      if (app_type == "StdoutLogAppender") {
        auto pAppender = std::make_shared<StdoutLogAppender>();
        pAppender->setLevel(
          LogLevel::fromString(app_node["level"].as<std::string>()));
        if (app_node["formatter"].IsDefined()
            && !app_node["formatter"].IsNull()) {
          pAppender->setFormatter(std::make_shared<LogFormatter>(
            app_node["formatter"]["pattern"].as<std::string>()));
        }
        pLogger->addAppender(pAppender);
      }
      else if (app_type == "SyncFileLogAppender") {
        auto pAppender = std::make_shared<FileLogAppender>(
          app_node["filename"].as<std::string>());
        pAppender->setLevel(
          LogLevel::fromString(app_node["level"].as<std::string>()));
        if (app_node["formatter"].IsDefined()
            && !app_node["formatter"].IsNull()) {
          pAppender->setFormatter(std::make_shared<LogFormatter>(
            app_node["formatter"]["pattern"].as<std::string>()));
        }
        pLogger->addAppender(pAppender);
      }
      else if (app_type == "AsyncFileLogAppender") {
        auto pAppender = std::make_shared<AsyncFileLogAppender>(
          app_node["filename"].as<std::string>());
        pAppender->setLevel(
          LogLevel::fromString(app_node["level"].as<std::string>()));
        if (app_node["formatter"].IsDefined()
            && !app_node["formatter"].IsNull()) {
          pAppender->setFormatter(std::make_shared<LogFormatter>(
            app_node["formatter"]["pattern"].as<std::string>()));
        }
        pLogger->addAppender(pAppender);
      }
      pLogger->setParent(GET_LOGGER(cur["parent"].as<std::string>()));
    }
    LogManager::instance()->putLogger(pLogger);
  }
}
