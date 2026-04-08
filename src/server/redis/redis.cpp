#include "../../../include/server/redis/redis.hpp"
#include "../../../include/server/logger.h"

#include <hiredis/hiredis.h>
#include <hiredis/read.h>
#include <mutex>
#include <poll.h>
#include <unistd.h>

// 构造函数
Redis::Redis()
    : _publishContext(nullptr), _subscribeContext(nullptr), _running(false) {
  _wakeupFds[0] = -1;
  _wakeupFds[1] = -1;
}

// 析构函数
Redis::~Redis() { stop(); }

// 初始化发布上下文连接
bool Redis::initPublishContext() {
  // 使用redisConnect函数连接redis服务器，返回一个redisContext指针
  _publishContext = redisConnect("127.0.0.1", 6379);
  if (_publishContext == nullptr) {
    LOG_ERROR("init publish context failed: context is nullptr");
    return false;
  }

  // 返回的redisContext指针不为nullptr并不能代表redis服务器连接成功
  // redisConnect函数连接redis服务器失败时，会返回一个redisContext指针，但其err成员会被设置为非0，表示发生了错误,此时可以通过errstr成员获取错误信息
  if (_publishContext->err) {
    LOG_ERROR("init publish context failed: %s", _publishContext->errstr);
    return false;
  }

  redisReply *reply =
      (redisReply *)redisCommand(_publishContext, "AUTH %s", "wang112233");

  if (reply == nullptr) {
    LOG_ERROR("publish context auth failed: reply is nullptr");
    redisFree(_publishContext);
    _publishContext = nullptr;
    return false;
  }

  if (reply->type == REDIS_REPLY_ERROR) {
    LOG_ERROR("publish context auth failed: %s", reply->str);
    freeReplyObject(reply);
    redisFree(_publishContext);
    _publishContext = nullptr;
    return false;
  }

  freeReplyObject(reply);

  return true;
}

// 初始化订阅上下文连接
bool Redis::initSubscribeContext() {
  _subscribeContext = redisConnect("127.0.0.1", 6379);
  if (_subscribeContext == nullptr) {
    LOG_ERROR("init subscribe context failed: context is nullptr");
    return false;
  }

  if (_subscribeContext->err) {
    LOG_ERROR("init subscribe context failed: %s", _subscribeContext->errstr);
    return false;
  }
  redisReply *reply =
      (redisReply *)redisCommand(_subscribeContext, "AUTH %s", "wang112233");

  if (reply == nullptr) {
    LOG_ERROR("subscribe context auth failed: reply is nullptr");
    redisFree(_subscribeContext);
    _subscribeContext = nullptr;
    return false;
  }

  if (reply->type == REDIS_REPLY_ERROR) {
    LOG_ERROR("subscribe context auth failed: %s", reply->str);
    freeReplyObject(reply);
    redisFree(_subscribeContext);
    _subscribeContext = nullptr;
    return false;
  }

  freeReplyObject(reply);

  return true;
}

// 清空redis上下文
void Redis::reset() {
  if (_publishContext != nullptr) {
    // 使用redisFree函数释放redisContext指针指向的资源，并将指针置为nullptr
    redisFree(_publishContext);
    _publishContext = nullptr;
  }

  if (_subscribeContext != nullptr) {
    redisFree(_subscribeContext);
    _subscribeContext = nullptr;
  }

  // 重置文件描述符组
  if (_wakeupFds[0] != -1) {
    // 释放对应的文件描述符资源，并将其置为-1，表示无效
    close(_wakeupFds[0]);
    _wakeupFds[0] = -1;
  }

  if (_wakeupFds[1] != -1) {
    close(_wakeupFds[1]);
    _wakeupFds[1] = -1;
  }

  // 清空安全队列，这里需要加锁，因为安全队列可能被多个线程访问
  {
    std::lock_guard<std::mutex> lock(_commandMutex);
    std::queue<Command> empty;
    std::swap(_commandQueue, empty);
  }

  // 清空订阅频道集合
  _subscribedChannels.clear();
  // 重置消息处理回调函数
  _messageHandler = nullptr;
  // 重置运行状态
  _running.store(false);
}

bool Redis::start(MessageHandler handler) {
  // 检查是不是已经在运行了，如果是，则直接返回false
  // 原子级变量内的load函数可以获取当前的值，如果当前值为true，说明已经在运行了，此时直接返回false，表示启动失败

  if (_running.load()) {
    LOG_ERROR("redis module is already running");
    return false;
  }

  // 判断回调函数是否为空，如果为空，则直接返回false，表示启动失败
  if (!handler) {
    LOG_ERROR("message handler is empty");
    return false;
  }

  _messageHandler = handler;

  // 初始化上下文连接，如果初始化失败，则直接返回false，表示启动失败
  if (!initPublishContext()) {
    // 既然上下文连接初始化失败了并且之前已经对redis模块进行了初始化，那么就需要清空redis模块，释放资源，避免内存泄漏
    reset();
    return false;
  }
  if (!initSubscribeContext()) {
    reset();
    return false;
  }

  // 初始化唤醒监听线程的管道文件描述符组，如果初始化失败，则直接返回false，表示启动失败
  // pipe系统调用可以创建一个管道，并返回两个文件描述符，分别用于读和写，如果pipe函数调用失败了，会返回-1，此时需要清空redis模块，释放资源，避免内存泄漏
  if (pipe(_wakeupFds) == -1) {
    LOG_ERROR("create wakeup pipe failed");
    reset();
    return false;
  }

  // 启动redis模块
  _running.store(true);

  // 启动redis模块的监听线程，监听线程会一直运行，直到redis模块被停止
  try {
    _listenerThread = std::thread(&Redis::listenerLoop, this);
  } // std::exception是c++标准库中所有的异常类的基类，可以捕捉到所有的标准异常，如果在启动监听线程时发生了异常，比如std::system_error，说明线程创建失败了，此时需要清空redis模块，释放资源，避免内存泄漏
  catch (const std::exception &ex) {
    // 输出异常信息，说明启动监听线程失败了,std::exception 中的
    // what函数可以获取异常的具体信息
    LOG_ERROR("start listener thread failed: %s", ex.what());
    reset();
    return false;
  } catch (...) {
    LOG_ERROR("start listener thread failed: unknown error");
    reset();
    return false;
  }

  LOG_INFO("redis module started successfully");
  return true;
}

// 关闭redis模块
void Redis::stop() {
  // redis模块停止，检查是否是运行状态，如果不是，则直接返回，表示停止成功
  if (_running.load()) {
    // 向监听线程发送停止命令，唤醒监听线程，让监听线程退出
    enqueueCommand({CommandType::Stop, 0});
    // 唤醒监听线程
    wakeupListener();
  }

  // 等待监听线程退出，如果监听线程没有退出，那么就一直等待，直到监听线程退出了，才会继续执行下面的代码，清空redis模块，释放资源，避免内存泄漏
  // joinable函数可以判断线程是否可连接，如果监听线程已经退出了或者根本没有工作或者线程内没有函数，那么joinable函数会返回false，此时就不需要调用join函数了，如果监听线程存在，在工作，还没有退出，那么joinable函数会返回true，此时就需要调用join函数，等待监听线程退出了，才会继续执行下面的代码，清空redis模块，释放资源，避免内存泄漏
  if (_listenerThread.joinable()) {
    _listenerThread.join();
  }
  LOG_INFO("redis module stopped");
  reset();
}

void Redis::handleCommand(const Command &command) {
  switch (command.type) {
  case CommandType::Subscribe:
    LOG_INFO("handle subscribe command, channel=%d", command.channel);
    sendSubscribeCommand(command.channel);
    break;
  case CommandType::Unsubscribe:
    LOG_INFO("handle unsubscribe command, channel=%d", command.channel);
    sendUnsubscribeCommand(command.channel);
    break;
  case CommandType::Stop:
    LOG_INFO("handle stop command");
    _running.store(false);
    break;
  }
}

// 将外部线程发送的命令放入安全队列中
bool Redis::enqueueCommand(Command command) {
  // 1、加锁
  std::lock_guard<std::mutex> lock(_commandMutex);

  // 2、将命令放入安全队列中
  _commandQueue.push(command);
  return true;
}

// 唤醒监听线程
bool Redis::wakeupListener() {
  // 先判断写端文件描述符是否有效，如果无效了，说明监听线程已经退出了或者根本没有工作了，此时就不需要唤醒监听线程了，直接返回true，表示唤醒成功了
  if (_wakeupFds[1] == -1) {
    LOG_ERROR("wakeup write fd is invalid");
    return false;
  }

  char buf = 'w'; // 发送一个字节的数据，内容无所谓，只要能唤醒监听线程就行了
  ssize_t n = write(_wakeupFds[1], &buf, sizeof(buf));
  if (n == -1) {
    LOG_ERROR("write wakeup signal failed");
    return false;
  }

  return true;
}

// 向redis指定的通道subscribe订阅消息
bool Redis::subscribe(int channel) {
  // 1、判断redis模块是否在运行
  if (!_running.load()) {
    LOG_ERROR("redis module is not running");
    return false;
  }

  if (!enqueueCommand({CommandType::Subscribe, channel})) {
    return false;
  }
  return wakeupListener();
}
// 向redis指定的通道unsubscribe取消订阅消息
bool Redis::unsubscribe(int channel) {
  if (!_running.load()) {
    LOG_ERROR("redis module is not running");
    return false;
  }

  if (!enqueueCommand({CommandType::Unsubscribe, channel})) {
    return false;
  }

  return wakeupListener();
}

// 向redis指定的通道channel发布消息
bool Redis::publish(int channel, const std::string &message) {
  // 判断redis模块是否在运行
  if (!_running.load()) {
    LOG_ERROR("redis module is not running");
    return false;
  }

  // 先判断发布上下文是否有效
  if (_publishContext == nullptr) {
    LOG_ERROR("publish context is null");
    return false;
  }

  // 发布消息前需要加保证锁线程安全
  std::lock_guard<std::mutex> lock(_publishMutex);

  redisReply *reply = static_cast<redisReply *>(
      redisCommand(_publishContext, "PUBLISH %d %s", channel, message.c_str()));

  if (reply == nullptr) {
    LOG_ERROR("publish command failed, channel=%d", channel);
    return false;
  }

  if (reply->type == REDIS_REPLY_ERROR) {
    LOG_ERROR("publish command failed: %s", reply->str);
    freeReplyObject(reply);
    return false;
  }

  LOG_INFO("publish success, channel=%d", channel);
  freeReplyObject(reply);
  return true;
}

// 批量取走安全队列中的命令 （锁内进行交换，锁外再慢慢处理）
void Redis::handlePendingCommands() {
  // 创建临时队列，用于swap安全队列中的命令
  std::queue<Command> pendingCommands;

  // 加锁，保护安全队列
  {
    std::lock_guard<std::mutex> lock(_commandMutex);
    // swap
    std::swap(_commandQueue, pendingCommands);
  }

  // 执行安全队列里的命令
  while (!pendingCommands.empty()) {
    handleCommand(pendingCommands.front());
    pendingCommands.pop();
  }
}

// 监听线程
void Redis::listenerLoop() {
  // 监听线程全权管理订阅上下文，如果监听线程没有也不需要进行
  if (_subscribeContext == nullptr) {
    LOG_ERROR("subscribe context is null");
    _running.store(false);
    return;
  }

  // 定义pollfd监听wakeupFds[0]唤醒监听线程和消息回调
  struct pollfd fds[2];
  fds[0].fd = _wakeupFds[0];
  fds[0].events = POLLIN; // 监听可读事件
  fds[0].revents = 0;

  fds[1].fd = _subscribeContext->fd; // 监听redis服务器发送的消息
  fds[1].events = POLLIN;            // 监听可读事件
  fds[1].revents = 0;

  while (_running.load()) {
    // 接收有几个fd响应了事件，而不在乎是哪个fd响应了事件，响应了多少次
    int ret = poll(fds, 2, -1);

    // ret<0，说明poll函数调用失败了
    if (ret < 0) {
      LOG_ERROR("poll failed");
      _running.store(false);
      break;
    }

    // 如果fds[0]的revents成员中包含POLLIN事件，说明监听线程被唤醒了，此时需要处理安全队列中的命令
    if (fds[0].revents & POLLIN) {
      char buffer[64] = {0};
      read(_wakeupFds[0], buffer,
           sizeof(buffer)); // 读取数据，清空管道，准备下一次唤醒
      // 处理安全队列中的命令
      handlePendingCommands();
    }

    // 上面有可能执行stop命令
    if (!_running.load()) {
      break;
    }

    // 如果fds[1]的revents成员中包含POLLIN事件，说明redis服务器发送了消息，此时需要处理订阅通道中的消息
    if (fds[1].revents & POLLIN) {
      redisReply *reply = nullptr;

      if (REDIS_OK ==
          redisGetReply(_subscribeContext, reinterpret_cast<void **>(&reply))) {
        handleRedisReply(reply);
      } else {
        LOG_ERROR("redisGetReply failed");
        _running.store(false);
        // 这里不能break，因为redisReply还没有释放
      }

      if (reply != nullptr) {
        freeReplyObject(reply);
      }
    }
  }
}

// 发送订阅命令
bool Redis::sendSubscribeCommand(int channel) {
  // 判断订阅上下文是否有效
  if (_subscribeContext == nullptr) {
    LOG_ERROR("subscribe context is null");
    return false;
  }

  // _subscribedChannels存储订阅频道表，如果channel已经存在了，则无需再次订阅
  if (_subscribedChannels.count(channel) > 0) {
    LOG_INFO("already subscribed to channel %d", channel);
    return true;
  }

  // 将发布订阅命令放入redis命令缓冲区
  if (REDIS_ERR ==
      redisAppendCommand(_subscribeContext, "SUBSCRIBE %d", channel)) {
    LOG_ERROR("send subscribe command failed, channel=%d", channel);
    return false;
  }

  int done = 0;
  while (!done) {
    // 将redis命令缓冲区中的命令发送给网络，目标是redis服务器
    if (REDIS_ERR == redisBufferWrite(_subscribeContext, &done))
    // done 被修改为 1：说明缓冲区里的数据全部发送到网络了
    // done 被修改为 0：说明网络通道暂时满了，缓冲区里还有数据！
    {
      LOG_ERROR("write subscribe command failed, channel=%d", channel);
      return false;
    }
  }

  LOG_INFO("subscribe command sent, channel=%d", channel);
  return true;
}

// 发送取消订阅命令
bool Redis::sendUnsubscribeCommand(int channel) {
  // 判断订阅上下文是否有效
  if (_subscribeContext == nullptr) {
    LOG_ERROR("subscribe context is null");
    return false;
  }

  // 将取消订阅命令放入redis命令缓冲区
  if (REDIS_ERR ==
      redisAppendCommand(_subscribeContext, "UNSUBSCRIBE %d", channel)) {
    LOG_ERROR("append unsubscribe command failed, channel=%d", channel);
    return false;
  }

  int done = 0;
  while (!done) {
    if (REDIS_ERR == redisBufferWrite(_subscribeContext, &done)) {
      LOG_ERROR("write unsubscribe command failed, channel=%d", channel);
      return false;
    }
  }

  LOG_INFO("unsubscribe command sent, channel=%d", channel);
  return true;
}

// 连接回调函数，处理订阅通道中的消息 ["subscribe", "123", 1] ["unsubscribe",
// "123", 1] ["message", "123", "hello"]
void Redis::handleRedisReply(redisReply *reply) {
  // 先判断reply是否有效
  if (reply == nullptr) {
    return;
  }

  if (reply->type == REDIS_REPLY_ERROR) {
    if (reply->str != nullptr) {
      LOG_ERROR("redis reply error: %s", reply->str);
    }
    return;
  }

  // Pub/Sub 的标准回复永远是一个数组,而且元素个数永远大于等于3
  if (reply->type != REDIS_REPLY_ARRAY || reply->elements < 3) {
    return;
  }

  // 判断 Pub/Sub 回复的第一个元素是否为空，第一个元素内的字符串是否为空
  if (reply->element[0] == nullptr || reply->element[0]->str == nullptr) {
    return;
  }

  // 获取 Pub/Sub 的第一个元素的字符串内容
  std::string replyType = reply->element[0]->str;

  if (replyType == "subscribe") {
    // 处理订阅消息
    if (reply->element[1] != nullptr && reply->element[1]->str != nullptr) {
      int channel = std::atoi(reply->element[1]->str);
      _subscribedChannels.insert(channel);
      LOG_INFO("subscribe confirmed, channel=%d", channel);
    }
    return;
  } else if (replyType == "unsubscribe") {
    // 处理取消订阅消息
    if (reply->element[1] != nullptr && reply->element[1]->str != nullptr) {
      int channel = std::atoi(reply->element[1]->str);
      _subscribedChannels.erase(channel);
      LOG_INFO("unsubscribe confirmed, channel=%d", channel);
    }
    return;
  } else if (replyType == "message") {
    // 处理消息
    if (_messageHandler == nullptr) {
      // 如果回调函数为定义了，那么就不处理消息了，直接返回
      return;
    }

    if (reply->element[1] == nullptr || reply->element[1]->str == nullptr) {
      return;
    }

    if (reply->element[2] == nullptr || reply->element[2]->str == nullptr) {
      return;
    }

    // 获取消息中的频道和消息内容，并调用回调函数处理消息
    int channel = std::atoi(reply->element[1]->str);
    std::string message = reply->element[2]->str;
    LOG_INFO("message received from redis, channel=%d", channel);
    _messageHandler(channel, message);
  }
}
