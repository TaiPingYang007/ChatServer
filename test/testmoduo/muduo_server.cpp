/*
muduo 网络库提供了两个主要的类

TcpServer：用于编写服务器程序的
TcpClient：用于编写客户端程序的

网络库就是对 epoll + 线程池 进行封装

好处：把网络I/O代码和业务代码区分开
                        用户的连接和断开    用户的可读写事件
*/
#include <iostream>
#include <mymuduo/EventLoop.h>
#include <mymuduo/TcpServer.h>
#include <string>

/* 基于muduo库网络库开发服务器程序
1、组合TcpServer对象
2、创建EventLoop事件循环对象的指针
3、明确TcpServer的构造函数参数：
    EventLoop *loop,循环对象
    const InetAddress &listenAddr,地址和端口号
    const std::string &nameArg,服务器名字
输出ChatServer的析构函数
4、在当前服务器类的构造函数当中，注册处理连接的回调函数和处理读写的回调函数
5、设置合适的服务端进程数量
*/

class ChatServer {
public:
  ChatServer(EventLoop *loop,               // 事件循环
             const InetAddress &listenAddr, // ip地址+端口
             const std::string &nameArg)    // 服务器的名字
      : _server(loop, listenAddr, nameArg), _loop(loop) {
    // 给服务器啊注册用户来连接的创建和断开回调
    _server.setConnectionCallback(
        [this](const TcpConnectionPtr &conn) { onConnection(conn); });
    // 给服务器注册用户读写时间回调
    _server.setMessageCallback(
        [this](const TcpConnectionPtr &conn, Buffer *buf, Timestamp time) {
          onMessage(conn, buf, time);
        });

    // 设置服务器端的线程数量  1个I/O线程  3个worker线程
    _server.setThreadNum(4);
  }

  // 开始时间循环
  void start() { _server.start(); }

private:
  // 专门处理用户的创建和断开 epoll listenfd accept
  void onConnection(const TcpConnectionPtr &conn) {
    // conn->connected() 判断是否连接成功 bool
    if (conn->connected()) {
      std::cout << conn->peerAddress().toIpPort() << "- >"
                << conn->localAddress().toIpPort() << "state：online" << "\n";
    } else {
      std::cout << conn->peerAddress().toIpPort() << "- >"
                << conn->localAddress().toIpPort() << "state：offline" << "\n";
      conn->shutdown(); // 关闭连接
      // _loop->quit();    连接断开
    }
  }

  // 专门处理用户的读写事件
  void onMessage(const TcpConnectionPtr &conn, // 连接
                 Buffer *buf,                  // 缓冲区
                 Timestamp time)               // 接收到数据的时间
  {
    std::string readBuf = buf->retrieveAllAsString();
    std::cout << "recv data :" << readBuf << "time" << time.toString() << "\n";
    conn->send(readBuf);
  }
  TcpServer _server; // #1
  EventLoop *_loop;  // #2 epoll
};

int main() {

  EventLoop loop; // epoll 事件循环对象
  InetAddress listenAddr(6000);
  ChatServer server(&loop, listenAddr, "ChatServer");

  server.start(); // 启动服务器  listenfd epoll_ctl => epoll
  loop.loop();    // 开始事件循环
               // epoll_wait以阻塞方式等待新用户的来凝结，已连接用户的读写事件等

  return 0;
}