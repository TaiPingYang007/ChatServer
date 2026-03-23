#include "../../include/server/chatservice.hpp"
#include "../../include/public.hpp"
#include <mutex>

// 获取单例对象的接口函数
ChatService *ChatService::instance() {
  static ChatService service;
  return &service;
}

// 注册消息以及对应的Handler回调操作
ChatService::ChatService() {
  _msgHandlerMap.insert(
      {static_cast<int>(EnMsgType::LOGIN_MSG),
       [this](const TcpConnectionPtr &conn, nlohmann::json &js,
              Timestamp time) { ChatService::login(conn, js, time); }});
  _msgHandlerMap.insert(
      {static_cast<int>(EnMsgType::LOGINOUT_MSG),
       [this](const TcpConnectionPtr &conn, nlohmann::json &js,
              Timestamp time) { loginout(conn, js, time); }});
  _msgHandlerMap.insert(
      {static_cast<int>(EnMsgType::REG_MSG),
       [this](const TcpConnectionPtr &conn, nlohmann::json &js,
              Timestamp time) { ChatService::reg(conn, js, time); }});
  _msgHandlerMap.insert(
      {static_cast<int>(EnMsgType::ONE_CHAT_MSG),
       [this](const TcpConnectionPtr &conn, nlohmann::json &js,
              Timestamp time) { ChatService::oneChat(conn, js, time); }});
  _msgHandlerMap.insert(
      {static_cast<int>(EnMsgType::ADD_FRIEND_REQUEST),
       [this](const TcpConnectionPtr &conn, nlohmann::json &js,
              Timestamp time) { ChatService::addFriend(conn, js, time); }});
  _msgHandlerMap.insert({static_cast<int>(EnMsgType::ADD_FRIEND_HANDLE),
                         [this](const TcpConnectionPtr &conn,
                                nlohmann::json &js, Timestamp time) {
                           ChatService::addFriendHandle(conn, js, time);
                         }});
  _msgHandlerMap.insert(
      {static_cast<int>(EnMsgType::CREATE_GROUP_MSG),
       [this](const TcpConnectionPtr &conn, nlohmann::json &js,
              Timestamp time) { ChatService::createGroup(conn, js, time); }});
  _msgHandlerMap.insert(
      {static_cast<int>(EnMsgType::ADD_GROUP_MSG),
       [this](const TcpConnectionPtr &conn, nlohmann::json &js,
              Timestamp time) { ChatService::addGroup(conn, js, time); }});
  _msgHandlerMap.insert({static_cast<int>(EnMsgType::ADD_GROUP_HANDLE),
                         [this](const TcpConnectionPtr &conn,
                                nlohmann::json &js, Timestamp time) {
                           ChatService::addGroupAccept(conn, js, time);
                         }});
  _msgHandlerMap.insert(
      {static_cast<int>(EnMsgType::GROUP_CHAT_MSG),
       [this](const TcpConnectionPtr &conn, nlohmann::json &js,
              Timestamp time) { ChatService::groupChat(conn, js, time); }});

  if (_redis.connect()) {
    _redis.init_notify_handler([this](int userid, std::string msg) {
      ChatService::handleRedisSubscribeMessage(userid, msg);
    });
  }
}

// 服务器异常，业务重置方法
void ChatService::reset() {
  // 1、把online状态的用户重置成offline
  _userModel.resetState();
}

// 获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgid) {
  // 记录错误日志（使用muduo库日志打印），msgid没有对应的事件处理回调
  // 如果是不存在的事件处理回调，会在_msgHandlerMap容器中添加一对新的数据，所以需要提前判断
  auto it = _msgHandlerMap.find(msgid);
  if (it == _msgHandlerMap.end()) {
    // 返回一个默认的处理器，空操作
    return [=](auto a, auto b, auto c) {
      LOG_ERROR("msgid：%d can not find handler!", msgid);
    };
  } else {
    return _msgHandlerMap[msgid];
  }
}

// 处理登录业务 name password 判断密码是否正确
/* 消息响应：
  登录成功，0
  登录用户已登陆，1
  密码错误，2
  用户不存在，3
*/
void ChatService::login(const TcpConnectionPtr &conn, nlohmann::json &js,
                        Timestamp time) {
  std::string name = js["name"];
  std::string password = js["password"];

  User user = _userModel.query(name);

  if (user.getName() == name && user.getPassword() == password) {
    if (user.getState() == "online") {
      // 该用户已登录，不允许重读登录
      nlohmann::json response;
      response = {{"msgid", static_cast<int>(EnMsgType::LOG_MSG_ACK)},
                  {"errno", 1},
                  {"errmsg", "this account is using, input another!\n"}};
      conn->send(response.dump());
    } else {
      // 登录成功，更新用户状态信息
      user.setState("online");
      _userModel.updateState(user);
      // 存储用户的通讯连接
      {
        std::lock_guard<std::mutex> lock(_connMutex);
        _userConnMap.insert({user.getId(), conn});
      }

      // 登录成功，订阅该用户的消息频道
      _redis.subscribe(user.getId());

      // 向用户返回登录成功信息
      nlohmann::json response;
      response = {{"msgid", static_cast<int>(EnMsgType::LOG_MSG_ACK)},
                  {"errno", 0},
                  {"id", user.getId()},
                  {"name", user.getName()},
                  {"errmsg", "登陆成功！\n"}};
      // 查询用户是否有离线消息
      std::vector<std::string> vec = _offlineMsgModel.query(user.getId());
      if (!vec.empty()) {
        response["offlinemsg"] = vec;
        // 读取用户的离线消息后，把该用户的所有离线消息删除掉
        _offlineMsgModel.remove(user.getId());
      }

      // 查询该用户的好友信息并返回
      std::vector<User> userVec01 = _friendModel.query(user.getId());
      if (!userVec01.empty()) {
        std::vector<std::string> userVec02;
        for (User &user : userVec01) {
          nlohmann::json js;
          js["id"] = user.getId();
          js["name"] = user.getName();
          js["state"] = user.getState();
          userVec02.emplace_back(js.dump());
        }
        response["friends"] = userVec02;
      }

      // 查询该用户的群组信息并返回
      std::vector<Group> groupVec01 = _groupModel.queryGroup(user.getId());
      if (!groupVec01.empty()) {
        std::vector<std::string> groupVec02;
        for (Group &group : groupVec01) {
          nlohmann::json js;
          js["id"] = group.getId();
          js["name"] = group.getName();
          js["desc"] = group.getDesc();
          // 自定义数据类型users怎么返回？
          // 再创建一个字符串容器，来存放json字符串
          std::vector<std::string> uservec;
          for (GroupUser &user : group.getUsers()) {
            nlohmann::json userjs;
            userjs["id"] = user.getId();
            userjs["name"] = user.getName();
            userjs["state"] = user.getState();
            userjs["role"] = user.getRole();
            uservec.emplace_back(userjs.dump());
          }
          js["users"] = uservec;
          groupVec02.emplace_back(js.dump());
        }
        response["groups"] = groupVec02;
      }

      conn->send(response.dump());
    }
  } else {
    // 该用户不存在
    if (user.getName().empty()) {
      nlohmann::json response;
      response = {{"msgid", static_cast<int>(EnMsgType::LOG_MSG_ACK)},
                  {"errno", 3},
                  {"errmsg", "User does not exist. Please register first.\n"}};
      conn->send(response.dump());
    } else {
      nlohmann::json response;
      response = {{"msgid", static_cast<int>(EnMsgType::LOG_MSG_ACK)},
                  {"errno", 2},
                  {"errmsg", "Incorrect password. Please try again.\n"}};
      conn->send(response.dump());
    }
  }
}

// 处理登出业务
void ChatService::loginout(const TcpConnectionPtr &conn, nlohmann::json &js,
                           Timestamp time) {
  int userid = js["userid"].get<int>();

  // 操作_UserConnMap表
  {
    std::lock_guard<std::mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    if (it != _userConnMap.end()) {
      _userConnMap.erase(userid);
    }
  }

  // 登出成功，取消订阅该用户的消息频道
  _redis.unsubscribe(userid);

  // 更新用户状态信息
  User user(userid, "", "", "offline");
  _userModel.updateState(user);
}

// 处理注册业务 name password
void ChatService::reg(const TcpConnectionPtr &conn, nlohmann::json &js,
                      Timestamp time) {
  std::string name = js["name"];
  std::string password = js["password"];

  User user;
  user.setName(name);
  user.setPassword(password);

  if (_userModel.insert(user)) {
    // 注册成功
    nlohmann::json response;
    response = {{"msgid", static_cast<int>(EnMsgType::REG_MSG_ACK)},
                {"errno", 0},
                {"id", user.getId()},
                {"errmsg", "Registration successful!\n"}};
    conn->send(response.dump());
  } else {
    // 注册失败
    nlohmann::json response;
    response = {{"msgid", static_cast<int>(EnMsgType::REG_MSG_ACK)},
                {"errno", 1},
                {"errmsg", "Registration failed!\n"}};
    conn->send(response.dump());
  }
}

// 处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr &conn) {
  // 创建user用户，用于存储下线用户的ID信息和修改状态
  User user;
  // 1、加锁，防止资源竞争
  {
    std::lock_guard<std::mutex> lock(_connMutex);

    // 2、遍历_userConnMap容器，找到对应的用户
    for (auto &x : _userConnMap) {
      if (x.second == conn) {
        // 3、将对应的键值对_userConnMap中删除
        user.setId(x.first);
        _userConnMap.erase(x.first);
        break;
      }
    }
  }

  // 取消订阅该用户的消息频道
  _redis.unsubscribe(user.getId());

  // 4、更新用户的状态信息
  if (user.getId() != -1) {
    user.setState("offline");
    _userModel.updateState(user);
  }
}

// 一对一聊天业务
void ChatService::oneChat(const TcpConnectionPtr &conn, nlohmann::json &js,
                          Timestamp time) {
  int userid = js["userid"].get<int>();
  int toid = js["to"].get<int>();

  if (!_friendModel.isFriend(userid, toid)) {
    // userid和toid之间不是好友，不允许发消息
    nlohmann::json response;

    response["errno"] = 1;
    response["msgid"] = static_cast<int>(EnMsgType::ONE_CHAT_MSG_ACK);
    response["errmsg"] =
        "You are not friends with this user. Message cannot be sent!";

    conn->send(response.dump());
    return;
  }

  {
    std::lock_guard<std::mutex> lock(_connMutex);
    auto it = _userConnMap.find(toid);
    if (it != _userConnMap.end()) {
      // toid在线，转发消息 服务器主动推送消息给to用户
      it->second->send(js.dump());
      return;
    }
  }

  // 查询toid是否在线
  User user = _userModel.query(toid);
  if (user.getState() == "online") {
    // toid在线，不在同一个服务器
    _redis.publish(toid, js.dump());
    return;
  }

  // toid不在线，存储离线消息
  _offlineMsgModel.insert(toid, js.dump());
}

// 添加好友业务请求
void ChatService::addFriend(const TcpConnectionPtr &conn, nlohmann::json &js,
                            Timestamp time) {
  int userid = js["userid"].get<int>();
  int friendid = js["friendid"].get<int>();

  // 判断好友是否存在
  if (!_friendModel.isUserExist(friendid)) {
    // 好友不存在
    nlohmann::json response;
    response["msgid"] = static_cast<int>(EnMsgType::ADD_FRIEND_RESPONSE);
    response["errno"] = 1;
    response["errmsg"] = "User does not exist!";
    conn->send(response.dump());
    return;
  }

  // 判断是否已经是好友了
  if (_friendModel.isFriend(userid, friendid)) {
    nlohmann::json response;
    response["msgid"] = static_cast<int>(EnMsgType::ADD_FRIEND_RESPONSE);
    response["errno"] = 2;
    response["errmsg"] = "This user is already your friend!";
    conn->send(response.dump());
    return;
  }

  // 验证通过！向目标用户发送好友申请 (服务器转发 ADD_FRIEND_REQUEST)
  {
    std::lock_guard<std::mutex> lock(_connMutex);
    auto it = _userConnMap.find(friendid);
    if (it != _userConnMap.end()) {
      // 目标用户在线
      it->second->send(js.dump());
      return;
    }
  }

  // 查询friendid是否在线
  User user = _userModel.query(friendid);
  if (user.getState() == "online") {
    // friendid在线，不在同一个服务器
    _redis.publish(friendid, js.dump());
    return;
  }

  // friendid不在线，存储离线消息
  _offlineMsgModel.insert(friendid, js.dump());
}

// 处理同意/拒绝添加好友的业务
void ChatService::addFriendHandle(const TcpConnectionPtr &conn,
                                  nlohmann::json &js, Timestamp time) {
  int userid = js["userid"].get<int>();     // 申请人 A 的 ID
  int friendid = js["friendid"].get<int>(); // 被申请人 B 的 ID
  std::string action = js["action"];

  nlohmann::json response;
  response["msgid"] = static_cast<int>(EnMsgType::ADD_FRIEND_RESPONSE);

  if (action == "accept") {
    // 更新好友关系表
    _friendModel.insert(userid, friendid);
    _friendModel.insert(friendid, userid);
    response["errno"] = 0;
    response["errmsg"] =
        "The user has accepted your friend request. You can now chat!";
  } else {
    // 拒绝
    response["errno"] = 3;
    response["errmsg"] = "The user has declined your friend request.";
  }

  // 统一通过响应通知用户 A
  {
    std::lock_guard<std::mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid); // 始终发送给 A
    if (it != _userConnMap.end()) {
      it->second->send(response.dump());
      return;
    }
  }
  // 查询userid是否在线
  User user = _userModel.query(userid);
  if (user.getState() == "online") {
    // userid在线，不在同一个服务器
    _redis.publish(userid, response.dump());
    return;
  }

  // userid不在线，存储离线消息
  _offlineMsgModel.insert(userid, response.dump());
}

// 创建群组业务 userid(创建人) groupname groupdesc
void ChatService::createGroup(const TcpConnectionPtr &conn, nlohmann::json &js,
                              Timestamp time) {
  int userid = js["userid"].get<int>();
  std::string name = js["groupname"];
  std::string groupdesc = js["groupdesc"];

  Group group(-1, name, groupdesc);

  if (_groupModel.createGroup(group)) {
    _groupModel.addGroup(userid, group.getId(), "creator");
  }
}

// 加入群组业务
void ChatService::addGroup(const TcpConnectionPtr &conn, nlohmann::json &js,
                           Timestamp time) {
  // 获取群组id
  int groupid = js["groupid"].get<int>();

  // 获取群主id
  int groupOwnerId = _groupModel.queryGroupOwnerId(groupid);
  if (groupOwnerId != -1) {
    // 找到群主，发送请求消息
    {
      std::lock_guard<std::mutex> lock(_connMutex);
      auto it = _userConnMap.find(groupOwnerId);
      if (it != _userConnMap.end()) {
        it->second->send(js.dump());
        return;
      }
    }

    // 查询groupOwnerId是否在线
    User user = _userModel.query(groupOwnerId);
    if (user.getState() == "online") {
      // groupOwnerId在线，不在同一个服务器
      _redis.publish(groupOwnerId, js.dump());
      return;
    }

    // groupOwnerId不在线，存储离线消息
    _offlineMsgModel.insert(groupOwnerId, js.dump());
  }
};

// 处理加群申请(同意/拒绝)业务
void ChatService::addGroupAccept(const TcpConnectionPtr &conn,
                                 nlohmann::json &js, Timestamp time) {
  int userid = js["userid"].get<int>();
  int groupid = js["groupid"].get<int>();

  // 接受群主选择
  std::string action = js["action"];

  nlohmann::json response;
  // 通知请求用户，群组申请结果
  response["msgid"] = static_cast<int>(EnMsgType::ADD_GROUP_RESPONSE);

  if (action == "accept") {
    // 加入群组
    _groupModel.addGroup(userid, groupid, "normal");
    response["errno"] = 0;
    response["msg"] =
        "Your request to join the group has been accepted. You can now chat!";
  } else {
    response["errno"] = 1;
    response["msg"] =
        "Your request to join the group has been rejected. You can not chat!";
  }

  // 查询用户在线表前上锁
  std::lock_guard<std::mutex> lock(_connMutex);
  auto it = _userConnMap.find(userid);

  if (it != _userConnMap.end()) {
    it->second->send(response.dump());
    return;
  }

  // 查询userid是否在线
  User user = _userModel.query(userid);
  if (user.getState() == "online") {
    // userid在线，不在同一个服务器
    _redis.publish(userid, response.dump());
    return;
  }

  // userid不在线，存储离线消息
  _offlineMsgModel.insert(userid, response.dump());
}

// 群聊业务
void ChatService::groupChat(const TcpConnectionPtr &conn, nlohmann::json &js,
                            Timestamp time) {
  // 获取群聊发送者id
  int userid = js["userid"].get<int>();
  // 获取群聊id
  int groupid = js["groupid"].get<int>();

  js["groupname"] = _groupModel.queryGroupName(groupid);

  // 获取群聊内的其他成员id
  std::vector<int> useridVec = _groupModel.queryGroupUsers(userid, groupid);

  // 群发消息前上锁
  std::lock_guard<std::mutex> lock(_connMutex);
  for (int &id : useridVec) {
    auto it = _userConnMap.find(id);
    if (it != _userConnMap.end()) {
      it->second->send(js.dump());
    } else {
      // 查询id是否在线
      User user = _userModel.query(id);
      if (user.getState() == "online") {
        // id在线，不在同一个服务器
        _redis.publish(id, js.dump());
        return;
      }

      // id不在线，存储离线消息
      _offlineMsgModel.insert(id, js.dump());
    }
  }
}

// 从redis消息队列中获取订阅消息
void ChatService::handleRedisSubscribeMessage(int userid, std::string msg) {
  std::lock_guard<std::mutex> lock(_connMutex);

  auto it = _userConnMap.find(userid);
  if (it != _userConnMap.end()) {
    it->second->send(msg);
    return;
  }

  // 存储用户的离线消息
  _offlineMsgModel.insert(userid, msg);
}