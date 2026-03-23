#include "../../include/server/chatserver.hpp"
#include "../../include/server/chatservice.hpp"
#include <mymuduo/Callbacks.h>

// 初始化聊天服务系统对象
ChatServer::ChatServer(EventLoop *loop, const InetAddress &listenAddr,
                       const std::string &nameArg)
    : _server(loop, listenAddr, nameArg), _loop(loop) {
  // 注册连接回调
  _server.setConnectionCallback(
      [this](const TcpConnectionPtr conn) { onConnection(conn); });
  // 注册消息回调
  _server.setMessageCallback(
      [this](const TcpConnectionPtr conn, Buffer *buf, Timestamp readTime) {
        onMessage(conn, buf, readTime);
      });
  // 设置线程数量
  _server.setThreadNum(4);
}

// 启动服务
void ChatServer::start() { _server.start(); }

// 上报连接相关的回调函数
void ChatServer::onConnection(const TcpConnectionPtr &conn) {

  // 客户端断开连接
  if (!conn->connected()) {
    ChatService::instance()->clientCloseException(conn);
    conn->shutdown();
  }
}

// 上报读写事件相关信息的回调函数
void ChatServer::onMessage(const TcpConnectionPtr &conn, Buffer *buf,
                           Timestamp time) {
  std::string readbuf = buf->retrieveAllAsString();

  try {
    // 数据的反序列化
    nlohmann::json js = nlohmann::json::parse(readbuf);

    // 防范：有没有传 msgid？
    if (!js.contains("msgid")) {
      LOG_ERROR("missing msgid in json: '%s'\n", readbuf.c_str());
      return;
    }
    // 目的是完全解耦网络模块的代码和业务模块的代码
    auto msgHandler =
        ChatService::instance()->getHandler(js["msgid"].get<int>());

    // 回调消息绑定好的事件处理器，来执行相应的业务逻辑
    msgHandler(conn, js, time);

  } catch (const nlohmann::json::parse_error &e) {
    LOG_ERROR("JSON parse error: %s. Data: %s\n", e.what(), readbuf.c_str());
  } catch (const nlohmann::json::type_error &e) {
    LOG_ERROR("JSON type conversion error: %s. Data: %s\n", e.what(),
              readbuf.c_str());
  } catch (const nlohmann::json::out_of_range &e) {
    LOG_ERROR("JSON map key not found: %s. Data: %s\n", e.what(),
              readbuf.c_str());
  } catch (...) {
    LOG_ERROR("Unknown exception caught in onMessage.\n");
  }
}
