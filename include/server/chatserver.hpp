#ifndef CHART_SERVER_HPP
#define CHART_SERVER_HPP

#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <string>
#include <mutex>

// 聊天服务器的主类
class ChatServer {
public:
  // 初始化聊天服务系统对象
  ChatServer(muduo::net::EventLoop *loop,
             const muduo::net::InetAddress &listenAddr,
             const std::string &nameArg);
  // 启动服务
  void start();

private:
  // 上报连接相关的回调函数
  void onConnection(const muduo::net::TcpConnectionPtr &conn);

  // key: TcpConnectionPtr  value: 连接尚未处理完的字符串缓冲区
  std::unordered_map<muduo::net::TcpConnectionPtr, std::string> _recvBufMap;
  // 保护 _recvBufMap 的线程安全
  std::mutex _recvBufMutex;

  // 上报读写事件相关信息的回调函数
  void onMessage(const muduo::net::TcpConnectionPtr &conn,
                 muduo::net::Buffer *buf,
                 muduo::Timestamp time); // Timestamp 在 muduo:: 而非 muduo::net::

  muduo::net::TcpServer  _server; // 组合的 muduo 库，实现服务器功能的类对象
  muduo::net::EventLoop *_loop;   // 指向事件循环对象的指针
};

#endif