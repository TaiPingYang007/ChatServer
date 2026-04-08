#ifndef CHART_SERVER_HPP
#define CHART_SERVER_HPP

#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <string>
#include <mutex>

// 引入 muduo 命名空间中的网络类型，避免代码中到处写 muduo::net::
using muduo::net::Buffer;
using muduo::net::EventLoop;
using muduo::net::InetAddress;
using muduo::net::TcpConnectionPtr;
using muduo::net::TcpServer;
using muduo::Timestamp; // Timestamp 在 muduo:: 而非 muduo::net::

// 聊天服务器的主类
class ChatServer {
public:
  // 初始化聊天服务系统对象
  ChatServer(EventLoop *loop, const InetAddress &listenAddr,
             const std::string &nameArg);
  // 启动服务
  void start();

private:
  // 上报连接相关的回调函数
  void onConnection(const TcpConnectionPtr &conn);

  // key: TcpConnectionPtr value: 连接尚未处理完的字符串缓冲区
  std::unordered_map<TcpConnectionPtr, std::string> _recvBufMap;
  // 保护_recvBufMap的线程安全  
  std::mutex _recvBufMutex;

  // 上报读写事件相关信息的回调函数
  void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time);
  TcpServer _server; // 组合的muduo库，实现服务器功能的类对象
  EventLoop *_loop;  // 指向事件循环对象的指针
};

#endif