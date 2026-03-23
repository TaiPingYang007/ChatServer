#ifndef CHAT_SERVICE_H
#define CHAT_SERVICE_H

#include "../../include/server/redis/redis.hpp"
#include "model/friendmodel.hpp"
#include "model/groupmodel.hpp"
#include "model/offlinemessagemodel.hpp"
#include "model/usermodel.hpp"
#include <mutex>
#include <mymuduo/Callbacks.h>
#include <mymuduo/Logger.h>
#include <mymuduo/TcpConnection.h>
#include <mymuduo/Timestamp.h>
#include <nlohmann/json.hpp>
#include <sys/socket.h>
#include <unordered_map>

// 表示处理消息的时间回调的方法类型
using MsgHandler = std::function<void(const TcpConnectionPtr &conn,
                                      nlohmann::json &js, Timestamp time)>;
// 聊天服务器业务类
class ChatService {
public:
  // 获取单例对象的接口函数
  static ChatService *instance();

  // 处理登录业务
  void login(const TcpConnectionPtr &conn, nlohmann::json &js, Timestamp time);

  // 处理登出业务
  void loginout(const TcpConnectionPtr &conn, nlohmann::json &js,
                Timestamp time);
  // 处理注册业务
  void reg(const TcpConnectionPtr &conn, nlohmann::json &js, Timestamp time);

  // 一对一聊天业务
  void oneChat(const TcpConnectionPtr &conn, nlohmann::json &js,
               Timestamp time);

  // 添加好友业务
  void addFriend(const TcpConnectionPtr &conn, nlohmann::json &js,
                 Timestamp time);

  // 处理同意/拒绝添加好友的业务
  void addFriendHandle(const TcpConnectionPtr &conn, nlohmann::json &js,
                       Timestamp time);

  // 创建群组业务
  void createGroup(const TcpConnectionPtr &conn, nlohmann::json &js,
                   Timestamp time);
  // 加入群组业务
  void addGroup(const TcpConnectionPtr &conn, nlohmann::json &js,
                Timestamp time);
  // 同意加入群组业务
  void addGroupAccept(const TcpConnectionPtr &conn, nlohmann::json &js,
                      Timestamp time);
  // 拒绝加入群组业务
  void addGroupReject(const TcpConnectionPtr &conn, nlohmann::json &js,
                      Timestamp time);
  // 群聊业务
  void groupChat(const TcpConnectionPtr &conn, nlohmann::json &js,
                 Timestamp time);

  // 获取消息对应的处理器
  MsgHandler getHandler(int msgid);

  // 服务器异常，业务重置方法
  void reset();

  // 处理客户端异常退出
  void clientCloseException(const TcpConnectionPtr &conn);

  // 从redis消息队列中获取消息
  void handleRedisSubscribeMessage(int userid, std::string msg);

private:
  ChatService();

  // 数据操作类对象
  UserModel _userModel;
  OfflineMsgModel _offlineMsgModel;
  FriendModel _friendModel;
  GroupModel _groupModel;

  // redis操作对象
  Redis _redis;

  // 存储在线用户的通讯连接
  std::unordered_map<int, TcpConnectionPtr> _userConnMap;

  // 定义互斥锁，保证_userConnMap的线程安全
  std::mutex _connMutex;

  // 存储消息id和其对应的业务处理方法
  std::unordered_map<int, MsgHandler> _msgHandlerMap;
};

#endif