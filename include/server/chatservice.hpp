#ifndef CHAT_SERVICE_H
#define CHAT_SERVICE_H

#include "../../include/server/redis/redis.hpp"
#include "model/friendmodel.hpp"
#include "model/groupmodel.hpp"
#include "../../include/server/model/friendrequestmodel.hpp"
#include "../../include/server/model/grouprequestmodel.hpp"
#include "model/offlinemessagemodel.hpp"
#include "model/usermodel.hpp"
#include <mutex>
#include <muduo/net/Callbacks.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/base/Timestamp.h>
#include <nlohmann/json.hpp>
#include <sys/socket.h>
#include <unordered_map>

// 表示处理消息的回调方法类型
// Timestamp 在 muduo:: 命名空间，网络类型在 muduo::net:: 命名空间
using MsgHandler = std::function<void(const muduo::net::TcpConnectionPtr &conn,
                                      nlohmann::json &js,
                                      muduo::Timestamp time)>;

// 聊天服务器业务类
class ChatService {
public:
  // 获取单例对象的接口函数
  static ChatService *instance();

  // 处理登录业务
  void login(const muduo::net::TcpConnectionPtr &conn, nlohmann::json &js,
             muduo::Timestamp time);

  // 处理登出业务
  void loginout(const muduo::net::TcpConnectionPtr &conn, nlohmann::json &js,
                muduo::Timestamp time);

  // 处理注册业务
  void reg(const muduo::net::TcpConnectionPtr &conn, nlohmann::json &js,
           muduo::Timestamp time);

  // 一对一聊天业务
  void oneChat(const muduo::net::TcpConnectionPtr &conn, nlohmann::json &js,
               muduo::Timestamp time);

  // 添加好友业务
  void addFriend(const muduo::net::TcpConnectionPtr &conn, nlohmann::json &js,
                 muduo::Timestamp time);

  // 处理同意/拒绝添加好友的业务
  void addFriendHandle(const muduo::net::TcpConnectionPtr &conn,
                       nlohmann::json &js, muduo::Timestamp time);

  // 创建群组业务
  void createGroup(const muduo::net::TcpConnectionPtr &conn, nlohmann::json &js,
                   muduo::Timestamp time);

  // 请求加入群组业务
  void addGroup(const muduo::net::TcpConnectionPtr &conn, nlohmann::json &js,
                muduo::Timestamp time);

  // 处理加入群组业务
  void addGroupHandle(const muduo::net::TcpConnectionPtr &conn,
                      nlohmann::json &js, muduo::Timestamp time);

  // 群聊业务
  void groupChat(const muduo::net::TcpConnectionPtr &conn, nlohmann::json &js,
                 muduo::Timestamp time);

  // 获取消息对应的处理器
  MsgHandler getHandler(int msgid);

  // 服务器异常，业务重置方法
  void reset();

  // 处理客户端异常退出
  void clientCloseException(const muduo::net::TcpConnectionPtr &conn);

  // 从 redis 消息队列中获取消息
  void handleRedisSubscribeMessage(int userid, std::string msg);

private:
  ChatService();

  // 将消息投递给目标用户（本机连接 → Redis 跨服 → 离线存储）
  void deliverMessage(int targetId, const std::string &payload);

  // 数据操作类对象
  UserModel _userModel;
  OfflineMsgModel _offlineMsgModel;
  FriendModel _friendModel;
  GroupModel _groupModel;
  FriendRequestModel _friendRequestModel;
  GroupRequestModel _groupRequestModel;

  // redis 操作对象
  Redis _redis;

  // 存储在线用户的通讯连接
  std::unordered_map<int, muduo::net::TcpConnectionPtr> _userConnMap;

  // 定义互斥锁，保证 _userConnMap 的线程安全
  std::mutex _connMutex;

  // 存储消息 id 和其对应的业务处理方法
  std::unordered_map<int, MsgHandler> _msgHandlerMap;
};

#endif
