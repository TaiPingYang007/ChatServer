#include "../../include/server/logger.h"

#include <cerrno>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <sstream>
#include <sys/stat.h>
#include <thread>

namespace {
/*
time_t 是一个表示时间的类型，时间戳是从1970年1月1日00:00:00
UTC到某个时间点的秒数，time_t 就是用来存储这个秒数的类型。 tm
是一个结构体类型，表示一个时间点的各个组成部分，比如年、月、日、时、分、秒等。tm
结构体的定义如下：
*/

// tm是函数返回值！获取当前的时间的函数
tm GetLocalTime(time_t now) {
  tm timeInfo;
#if defined(                                                                   \
    _WIN32) // 判断当前系统是不是Windows系统，如果是Windows系统，使用localtime_s函数获取当前时间，否则使用localtime_r函数获取当前时间
  localtime_s(&timeInfo, &now);
#else  // 不在windows系统
  localtime_r(&now, &timeInfo);
#endif // 判断结束
  return timeInfo;
}

// EnsureDirectory 确保文件夹存在,如果不存在就创建新文件夹
bool EnsureDirectory(const std::string &path) {
  // 判断路径是否为空，如果为空，直接返回false，表示文件夹创建失败
  if (path.empty()) {
    return false;
  }

  // stat就相当于一个文件结构体，包含了文件的各种属性，比如文件类型、权限、大小、修改时间等，stat函数可以获取一个文件的属性信息，并将其存储在一个stat结构体中，函数原型如下：
  // int stat(const char *pathname, struct stat *statbuf);
  struct stat st;

  if (stat(path.c_str(), &st) ==
      0) // ps:Linux底层的规则就是0表示成功，-1表示失败
  {
    // st.st_mode是表示文件类型和权限的一个字段
    // S_ISDIR是一个宏函数，用来判断一个文件是不是一个目录
    return S_ISDIR(st.st_mode);
  }

  // 系统调用mkdir函数创建文件夹，函数原型如下：
  // int mkdir(const char *pathname, mode_t mode); 路径、权限
  if (mkdir(path.c_str(), 0755) == 0) {
    return true;
  }

  // 返回文件已经存在的错误码，表示文件夹创建失败
  // errno 是一个线程局部变量，用来保存最近一次的错误信息
  // 如果mkdir因为文件已存在失败了，errno就会等于EEXIST，经过 ==
  // 判断为true，表示文件已经存在,如果是其他错误则会因为不相等返回false，表示文件夹创建失败
  return errno == EEXIST;
}

// 构建日志路径
std::string BuildLogPath(const std::string &logDir,
                         const std::string &programName,
                         const std::string &instanceTag, const tm &timeInfo) {
  // std::ostringstream
  // 输出字符串流，可以像使用std::cout一样使用它来构建一个字符串，最后通过str()函数获取构建好的字符串
  std::ostringstream oss;
  oss << logDir << "/" << (timeInfo.tm_year + 1900) << "-";
  // width 设置接下来的输出宽度为2，fill 如果输出的内容不足2个字符，就在前面补0
  oss.width(2);
  oss.fill('0');
  // 这里月份如果是4，因为前面的宽度设置了2，很显然4这个数字就不足2个字符了，所以就会在前面补0，输出04
  oss << (timeInfo.tm_mon + 1) << "-";
  oss.width(2);
  oss.fill('0');
  oss << timeInfo.tm_mday;

  if (!programName.empty()) {
    oss << "-" << programName;
  }
  if (!instanceTag.empty()) {
    oss << "-" << instanceTag;
  }
  oss << ".log";
  return oss.str();
}
} // namespace

Logger::Logger()
    : _logDir("."), _programName("log"), _instanceTag(""), _initialized(false) {
  std::thread consumer([this]() {
    FILE *pf =
        nullptr; // 文件指针，用来指向当前打开的日志文件，初始值为nullptr，表示还没有打开任何日志文件
    std::string
        currentPath; // 当前日志文件的路径，初始值为空字符串，表示还没有打开任何日志文件

    while (true) {
      std::string msg = _lockQueue.Pop();

      time_t now = time(nullptr);
      tm nowtm = GetLocalTime(now);
      std::string logDir;
      std::string programName;
      std::string instanceTag;
      {
        std::lock_guard<std::mutex> lock(_configMutex);
        logDir = _initialized ? _logDir : ".";
        programName = _initialized ? _programName : "log";
        instanceTag = _initialized ? _instanceTag : "";
      }

      // 获取当前的日志文件路径
      const std::string filePath =
          BuildLogPath(logDir, programName, instanceTag, nowtm);

      // 文件路径不同说明：要不是没创建要不是日期已经修改了，需要切换日志文件了
      if (pf == nullptr || filePath != currentPath) {
        if (pf != nullptr) {
          fclose(pf);
        }

        pf = fopen(filePath.c_str(), "a+");
        if (pf == nullptr) {
          std::cerr << "open log file error: " << filePath << std::endl;
          std::exit(EXIT_FAILURE);
        }

        currentPath = filePath;
      }

      fputs(msg.c_str(), pf);
      fflush(pf);
    }
  });

  consumer.detach();
}

Logger &Logger::GetInstance() {
  static Logger logger;
  return logger;
}

bool Logger::Init(const std::string &logDir, const std::string &programName,
                  const std::string &instanceTag) {
  if (!EnsureDirectory(logDir)) {
    return false;
  }

  std::lock_guard<std::mutex> lock(_configMutex);
  _logDir = logDir;
  _programName = programName.empty() ? "log" : programName;
  _instanceTag = instanceTag;
  _initialized = true;
  return true;
}

void Logger::Log(LogLevel level, const std::string &msg) {
  time_t now = time(nullptr);
  tm nowtm = GetLocalTime(now);

  char buf[128] = {0};
  std::snprintf(buf, sizeof(buf), "[%s] %02d:%02d:%02d => ",
                (level == LogLevel::INFO ? "INFO" : "ERROR"), nowtm.tm_hour,
                nowtm.tm_min, nowtm.tm_sec);

  std::string logMsg(buf);
  logMsg += msg;
  logMsg += '\n';

  _lockQueue.Push(logMsg);
}
