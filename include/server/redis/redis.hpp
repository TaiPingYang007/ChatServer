#ifndef REDIS_H
#define REDIS_H

#include <atomic> // 线程安全的标志位
#include <functional>
#include <mutex> // 保护共享资源的互斥锁
#include <queue> // 用来做安全队列
#include <string>
#include <thread>        // 用来创建监听线程
#include <unordered_set> // 用来存储订阅的频道集合

#include <hiredis/hiredis.h>

class Redis {
public:
  // 定义回调函数体
  using MessageHandler =
      std::function<void(int channel, const std::string &message)>;

  // 构造函数
  Redis();
  // 析构函数
  ~Redis();

  // 删除拷贝构造函数和赋值运算符，禁止复制
  Redis(const Redis &) = delete;
  Redis &operator=(const Redis &) = delete;

  // 开启redis模块
  bool start(MessageHandler handler);
  // 关闭redis模块
  void stop();

  // 向redis指定的通道channel发布消息
  bool publish(int channel,const std::string &message);

  // 向redis指定的通道subscribe订阅消息
  bool subscribe(int channel);

  // 向redis指定的通道unsubscribe取消订阅消息
  bool unsubscribe(int channel);

private:
  // 定义枚举类型，表示当前操作
  enum class CommandType {
    Subscribe,
    Unsubscribe,
    Stop
  };

  // 定义命令结构体，表示一个操作命令
  struct Command {
    CommandType type;
    int channel;
  };

  // 监听线程
  void listenerLoop();

  // 初始化发布上下文连接
  bool initPublishContext();
  // 初始化订阅上下文连接
  bool initSubscribeContext();
  // 清空redis上下文
  void reset();

  // 将外部线程发送的命令放入安全队列中
  bool enqueueCommand(Command command);
  // 唤醒监听线程
  bool wakeupListener();
  
  // 批量取走安全队列中的命令
  void handlePendingCommands();
  // 执行命令
  void handleCommand(const Command &command);

  // 发送订阅命令
  bool sendSubscribeCommand(int channel);
  // 发送取消订阅命令
  bool sendUnsubscribeCommand(int channel);

  // 连接回调函数，处理订阅通道中的消息
  void handleRedisReply(redisReply *reply);

private:
  // 发布上下文连接
  redisContext *_publishContext;
  // 订阅上下文连接
  redisContext *_subscribeContext;

  int _wakeupFds[2]; // 用于唤醒监听线程的管道文件描述符

  // 监听线程运行标志位
  std::atomic<bool> _running;
  std::thread _listenerThread;

  // 保护发布上下文连接的互斥锁
  std::mutex _publishMutex;

  // 保护安全队列的互斥锁
  std::mutex _commandMutex;
  // 安全队列，存储外部线程发送的命令
  std::queue<Command> _commandQueue;

  // 记录订阅的频道集合，方便取消订阅时查找,仅监听线程维护
  std::unordered_set<int> _subscribedChannels;

  // 消息处理回调函数
  MessageHandler _messageHandler;
};
#endif
