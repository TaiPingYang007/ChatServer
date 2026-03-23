#ifndef CHART_SERVER_HPP
#define CHART_SERVER_HPP

#include <mymuduo/EventLoop.h>
#include <mymuduo/TcpServer.h>
#include <nlohmann/json.hpp>

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

  // 上报读写事件相关信息的回调函数
  void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time);
  TcpServer _server; // 组合的muduo库，实现服务器功能的类对象
  EventLoop *_loop;  // 指向事件循环对象的指针
};

#endif