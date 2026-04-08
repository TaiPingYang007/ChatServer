#ifndef LOGGER_H
#define LOGGER_H

#include "lockqueue.h"
#include <cstdio>
#include <mutex>
#include <string>

enum class LogLevel {
  INFO,
  ERROR,
};

class Logger {
public:
  static Logger &GetInstance();

  bool Init(const std::string &logDir, const std::string &programName,
            const std::string &instanceTag);  // 文件路径 + 程序名称 + 实例标签（端口）
  void Log(LogLevel level, const std::string &msg); // 向日志队列中写入数据

private:
  Logger();
  Logger(const Logger &) = delete;
  Logger(Logger &&) = delete;
  Logger &operator=(const Logger &) = delete;
  Logger &operator=(Logger &&) = delete;

private:
  std::mutex _configMutex;
  std::string _logDir;  // 记录文件夹路径
  std::string _programName; // 记录程序名称
  std::string _instanceTag; // 记录实例标签，用来记录对应的服务器实例
  bool _initialized;  // 是否已经初始化
  LockQueue<std::string> _lockQueue;
};

// muduo/base/Logging.h 也定义了 LOG_INFO / LOG_ERROR（stream 风格），
// 先 undef 再重新定义为 printf 风格，确保我们的版本始终生效。
#ifdef LOG_INFO
#undef LOG_INFO
#endif
#ifdef LOG_ERROR
#undef LOG_ERROR
#endif

#define LOG_INFO(logmsgformat, ...)                                            \
  do {                                                                         \
    Logger &logger = Logger::GetInstance();                                    \
    char c[1024] = {0};                                                        \
    std::snprintf(c, sizeof(c), logmsgformat, ##__VA_ARGS__);                  \
    logger.Log(LogLevel::INFO, c);                                             \
  } while (0)

#define LOG_ERROR(logmsgformat, ...)                                           \
  do {                                                                         \
    Logger &logger = Logger::GetInstance();                                    \
    char c[1024] = {0};                                                        \
    std::snprintf(c, sizeof(c), logmsgformat, ##__VA_ARGS__);                  \
    logger.Log(LogLevel::ERROR, c);                                            \
  } while (0)

#endif
