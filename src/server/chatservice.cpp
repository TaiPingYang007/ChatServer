#include "../../include/server/chatservice.hpp"
#include "../../include/public.hpp"
#include "../../include/server/logger.h"
#include <cstdlib>
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
              Timestamp time) { login(conn, js, time); }});
  _msgHandlerMap.insert(
      {static_cast<int>(EnMsgType::LOGINOUT_MSG),
       [this](const TcpConnectionPtr &conn, nlohmann::json &js,
              Timestamp time) { loginout(conn, js, time); }});
  _msgHandlerMap.insert(
      {static_cast<int>(EnMsgType::REG_MSG),
       [this](const TcpConnectionPtr &conn, nlohmann::json &js,
              Timestamp time) { reg(conn, js, time); }});
  _msgHandlerMap.insert(
      {static_cast<int>(EnMsgType::ONE_CHAT_MSG),
       [this](const TcpConnectionPtr &conn, nlohmann::json &js,
              Timestamp time) { oneChat(conn, js, time); }});
  _msgHandlerMap.insert(
      {static_cast<int>(EnMsgType::ADD_FRIEND_REQUEST),
       [this](const TcpConnectionPtr &conn, nlohmann::json &js,
              Timestamp time) { addFriend(conn, js, time); }});
  _msgHandlerMap.insert(
      {static_cast<int>(EnMsgType::ADD_FRIEND_HANDLE),
       [this](const TcpConnectionPtr &conn, nlohmann::json &js,
              Timestamp time) { addFriendHandle(conn, js, time); }});
  _msgHandlerMap.insert(
      {static_cast<int>(EnMsgType::CREATE_GROUP_MSG),
       [this](const TcpConnectionPtr &conn, nlohmann::json &js,
              Timestamp time) { createGroup(conn, js, time); }});
  _msgHandlerMap.insert(
      {static_cast<int>(EnMsgType::ADD_GROUP_MSG),
       [this](const TcpConnectionPtr &conn, nlohmann::json &js,
              Timestamp time) { addGroup(conn, js, time); }});
  _msgHandlerMap.insert(
      {static_cast<int>(EnMsgType::ADD_GROUP_HANDLE),
       [this](const TcpConnectionPtr &conn, nlohmann::json &js,
              Timestamp time) { addGroupHandle(conn, js, time); }});
  _msgHandlerMap.insert(
      {static_cast<int>(EnMsgType::GROUP_CHAT_MSG),
       [this](const TcpConnectionPtr &conn, nlohmann::json &js,
              Timestamp time) { groupChat(conn, js, time); }});

  if (_redis.start([this](int userid, std::string msg) {
        ChatService::handleRedisSubscribeMessage(userid, msg);
      })) {
    LOG_INFO("Redis started successfully!");
  } else {
    LOG_ERROR("Failed to start Redis!");
    exit(EXIT_FAILURE);
  }
}

// 将消息投递给目标用户：优先走本机连接，其次 Redis 跨服，最后存离线
void ChatService::deliverMessage(int targetId, const std::string &payload) {
  {
    std::lock_guard<std::mutex> lock(_connMutex);
    auto it = _userConnMap.find(targetId);
    if (it != _userConnMap.end()) {
      it->second->send(payload + "\n");
      return;
    }
  }
  // 目标用户不在本机，查询是否在其他服务器上线
  User user = _userModel.query(targetId);
  if (user.getState() == "online") {
    _redis.publish(targetId, payload);
    return;
  }
  // 目标用户离线，存储离线消息
  _offlineMsgModel.insert(targetId, payload);
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
    return it->second;
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

  // 用户不存在（query 返回默认 User，name 为空）
  if (user.getName().empty()) {
    nlohmann::json response = {
        {"msgid", static_cast<int>(EnMsgType::LOG_MSG_ACK)},
        {"errno", 3},
        {"errmsg", "User does not exist. Please register first.\n"}};
    conn->send(response.dump() + "\n");
    return;
  }

  // 密码错误
  if (user.getPassword() != password) {
    nlohmann::json response = {
        {"msgid", static_cast<int>(EnMsgType::LOG_MSG_ACK)},
        {"errno", 2},
        {"errmsg", "Incorrect password. Please try again.\n"}};
    conn->send(response.dump() + "\n");
    return;
  }

  // 该用户已登录，不允许重复登录
  if (user.getState() == "online") {
    nlohmann::json response = {
        {"msgid", static_cast<int>(EnMsgType::LOG_MSG_ACK)},
        {"errno", 1},
        {"errmsg", "this account is using, input another!\n"}};
    conn->send(response.dump() + "\n");
    return;
  }

  // 登录成功：更新状态，注册连接，订阅 Redis
  user.setState("online");
  _userModel.updateState(user);
  {
    std::lock_guard<std::mutex> lock(_connMutex);
    _userConnMap.insert({user.getId(), conn});
  }
  _redis.subscribe(user.getId());

  // 向用户返回登录成功信息
  nlohmann::json response = {
      {"msgid", static_cast<int>(EnMsgType::LOG_MSG_ACK)},
      {"errno", 0},
      {"id", user.getId()},
      {"name", user.getName()},
      {"errmsg", "登陆成功！\n"}};

  // 查询用户是否有离线消息
  std::vector<std::string> offlineMsgs = _offlineMsgModel.query(user.getId());
  if (!offlineMsgs.empty()) {
    response["offlinemsg"] = offlineMsgs;
    // 读取用户的离线消息后，把该用户的所有离线消息删除掉
    _offlineMsgModel.remove(user.getId());
  }

  // 查询该用户的好友信息并返回
  std::vector<User> friends = _friendModel.query(user.getId());
  if (!friends.empty()) {
    std::vector<std::string> friendJsonList;
    for (User &friendUser : friends) {
      nlohmann::json js;
      js["id"] = friendUser.getId();
      js["name"] = friendUser.getName();
      js["state"] = friendUser.getState();
      friendJsonList.emplace_back(js.dump());
    }
    response["friends"] = friendJsonList;
  }

  // 查询该用户的群组信息并返回
  std::vector<Group> groups = _groupModel.queryGroup(user.getId());
  if (!groups.empty()) {
    std::vector<std::string> groupJsonList;
    for (Group &group : groups) {
      nlohmann::json js;
      js["id"] = group.getId();
      js["name"] = group.getName();
      js["desc"] = group.getDesc();
      // 群成员列表：每个成员序列化为 json 字符串后存入容器
      std::vector<std::string> uservec;
      for (GroupUser &groupUser : group.getUsers()) {
        nlohmann::json userjs;
        userjs["id"] = groupUser.getId();
        userjs["name"] = groupUser.getName();
        userjs["state"] = groupUser.getState();
        userjs["role"] = groupUser.getRole();
        uservec.emplace_back(userjs.dump());
      }
      js["users"] = uservec;
      groupJsonList.emplace_back(js.dump());
    }
    response["groups"] = groupJsonList;
  }

  conn->send(response.dump() + "\n");
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
                {"id", user.getId()}};
    conn->send(response.dump() + "\n");
  } else {
    // 注册失败
    nlohmann::json response;
    response = {{"msgid", static_cast<int>(EnMsgType::REG_MSG_ACK)},
                {"errno", 1}};
    conn->send(response.dump() + "\n");
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
    for (auto &entry : _userConnMap) {
      if (entry.second == conn) {
        // 3、将对应的键值对_userConnMap中删除
        user.setId(entry.first);
        _userConnMap.erase(entry.first);
        break;
      }
    }
  }

  // 取消订阅该用户的消息频道
  // _redis.unsubscribe(user.getId());
  // 客户端连接后没登录就断开，会往 Redis 发送无效的 UNSUBSCRIBE -1
  if (user.getId() != -1) {
    _redis.unsubscribe(user.getId());
    user.setState("offline");
    _userModel.updateState(user);
  }

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

    conn->send(response.dump() + "\n");
    return;
  }

  {
    std::lock_guard<std::mutex> lock(_connMutex);
    auto it = _userConnMap.find(toid);
    if (it != _userConnMap.end()) {
      // toid在线，转发消息 服务器主动推送消息给to用户
      it->second->send(js.dump() + "\n");
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

  // 判断添加的是不是自己
  if (userid == friendid) {
    nlohmann::json response;
    response["msgid"] = static_cast<int>(EnMsgType::ADD_FRIEND_RESPONSE);
    response["errno"] = 1;
    response["errmsg"] = "You cannot add yourself!";
    conn->send(response.dump() + "\n");
    return;
  }

  // 判断好友是否存在
  if (!_friendModel.isUserExist(friendid)) {
    // 好友不存在
    nlohmann::json response;
    response["msgid"] = static_cast<int>(EnMsgType::ADD_FRIEND_RESPONSE);
    response["errno"] = 1;
    response["errmsg"] = "User does not exist!";
    conn->send(response.dump() + "\n");
    return;
  }

  // 判断是否已经是好友了，是否已经是已接受状态
  if (_friendModel.isFriend(userid, friendid)) {
    nlohmann::json response;
    response["msgid"] = static_cast<int>(EnMsgType::ADD_FRIEND_RESPONSE);
    response["errno"] = 2;
    response["errmsg"] = "This user is already your friend!";
    conn->send(response.dump() + "\n");
    return;
  }

  RequestStatusResult result =
      _friendRequestModel.queryRequestStatus(userid, friendid);

  if (result.status == QueryStatus::DbError) {
    // 系统繁忙
    nlohmann::json response;
    response["msgid"] = static_cast<int>(EnMsgType::ADD_FRIEND_RESPONSE);
    response["errno"] = 2;
    response["errmsg"] = "System busy, please try again later.";
    conn->send(response.dump() + "\n");
    return;
  } else if (result.status == QueryStatus::NotFound) {
    // 如果是未写入数据库的好友请求，写入数据库
    if (!_friendRequestModel.addFriendRequest(userid, friendid)) {
      nlohmann::json response;
      response["msgid"] = static_cast<int>(EnMsgType::ADD_FRIEND_RESPONSE);
      response["errno"] = 5;
      response["errmsg"] = "System busy, please try again later.";
      conn->send(response.dump() + "\n");
      return;
    }
  } else if (result.status == QueryStatus::Ok) {
    // 判断是不是pending状态，如果是的话不允许重复发送
    if (result.value == "pending") {
      nlohmann::json response;
      response["msgid"] = static_cast<int>(EnMsgType::ADD_FRIEND_RESPONSE);
      response["errno"] = 2;
      response["errmsg"] = "The request has been sent!";
      conn->send(response.dump() + "\n");
      return;
    } else if (result.value == "rejected") {
      // 如果是拒绝状态应该更新状态并重新发送
      if (!_friendRequestModel.updateRequestStatus(userid, friendid,
                                                   "pending")) {
        nlohmann::json response;
        response["msgid"] = static_cast<int>(EnMsgType::ADD_FRIEND_RESPONSE);
        response["errno"] = 5;
        response["errmsg"] = "System busy, please try again later.";
        conn->send(response.dump() + "\n");
        return;
      }
    } else {
      LOG_ERROR("%s:%d: invalid FriendRequest status", __FILE_NAME__, __LINE__);
      return;
    }
  }

  // 验证通过！向目标用户发送好友申请 (服务器转发 ADD_FRIEND_REQUEST)
  deliverMessage(friendid, js.dump());
}

// 处理同意/拒绝添加好友的业务
void ChatService::addFriendHandle(const TcpConnectionPtr &conn,
                                  nlohmann::json &js, Timestamp time) {
  int userid = js["userid"].get<int>();     // 申请人 A 的 ID
  int friendid = js["friendid"].get<int>(); // 被申请人 B 的 ID
  std::string action = js["action"];

  nlohmann::json response;
  response["msgid"] = static_cast<int>(EnMsgType::ADD_FRIEND_RESPONSE);

  BoolQueryResult result =
      _friendRequestModel.isPendingRequest(userid, friendid);
  if (result.status == QueryStatus::DbError) {
    // 系统繁忙
    response["errno"] = 2;
    response["errmsg"] = "System busy, please try again later.";
    conn->send(response.dump() + "\n");
    return;
  } else if (result.status == QueryStatus::NotFound) // false
  {
    // 不是pending状态
    response["errno"] = 4;
    response["errmsg"] = "The user has not sent a friend request to you!";

    conn->send(response.dump() + "\n");
    return;
  } else if (result.status == QueryStatus::Ok) // true
  {
    // pending状态
    if (action == "accept") {
      // 更新好友请求表
      if (!_friendRequestModel.updateRequestStatus(userid, friendid,
                                                   "accepted")) {
        response["errno"] = 5;
        response["errmsg"] = "System busy, please try again later.";
        conn->send(response.dump() + "\n");
        return;
      }

      // 更新好友关系表
      _friendModel.insert(userid, friendid);
      _friendModel.insert(friendid, userid);
      response["errno"] = 0;
      response["errmsg"] =
          "The user has accepted your friend request. You can now chat!";
    } else {
      // 拒绝
      // 更新好友请求表
      _friendRequestModel.updateRequestStatus(userid, friendid, "rejected");

      response["errno"] = 3;
      response["errmsg"] = "The user has declined your friend request.";
    }

    // 统一通过响应通知申请人 用户A
    deliverMessage(userid, response.dump());
  }
}

// 创建群组业务 userid(创建人) groupname groupdesc
void ChatService::createGroup(const TcpConnectionPtr &conn, nlohmann::json &js,
                              Timestamp time) {
  int userid = js["userid"].get<int>();
  std::string name = js["groupname"];
  std::string groupdesc = js["groupdesc"];

  BoolQueryResult result = _groupModel.isGroupExist(name);
  if (result.status == QueryStatus::DbError) {
    // 系统繁忙
    nlohmann::json response;
    response["msgid"] = static_cast<int>(EnMsgType::CREATE_GROUP_MSG_ACK);
    response["errno"] = 2;
    response["errmsg"] = "System busy, please try again later.";
    conn->send(response.dump() + "\n");
    return;
  } else if (result.status == QueryStatus::NotFound) // false
  {
    Group group(-1, name, groupdesc);

    nlohmann::json response;
    if (_groupModel.createGroup(group)) {
      _groupModel.addGroup(userid, group.getId(), "creator");

      response["msgid"] = static_cast<int>(EnMsgType::CREATE_GROUP_MSG_ACK);
      response["errno"] = 0;
      response["errmsg"] = "Create group success! Your groupid is " +
                           std::to_string(group.getId());
    } else {
      response["msgid"] = static_cast<int>(EnMsgType::CREATE_GROUP_MSG_ACK);
      response["errno"] = 1;
      response["errmsg"] = "Create group failed!";
    }
    conn->send(response.dump() + "\n");
  } else if (result.status == QueryStatus::Ok) // true
  {
    // 组群名已存在或者其他错误
    nlohmann::json response;
    response["msgid"] = static_cast<int>(EnMsgType::CREATE_GROUP_MSG_ACK);
    response["errno"] = 2;
    response["errmsg"] = "Group name exists!";
    conn->send(response.dump() + "\n");
    return;
  }
}

// 加入群组业务
void ChatService::addGroup(const TcpConnectionPtr &conn, nlohmann::json &js,
                           Timestamp time) {
  int userid = js["userid"].get<int>();
  int groupid = js["groupid"].get<int>();

  // 检查用户是否已在群中
  BoolQueryResult inGroupResult = _groupModel.isUserInGroup(userid, groupid);
  if (inGroupResult.status == QueryStatus::DbError) {
    nlohmann::json response;
    response["msgid"] = static_cast<int>(EnMsgType::ADD_GROUP_RESPONSE);
    response["errno"] = 2;
    response["errmsg"] = "System busy, please try again later.";
    conn->send(response.dump() + "\n");
    return;
  }
  if (inGroupResult.status == QueryStatus::Ok) {
    nlohmann::json response;
    response["msgid"] = static_cast<int>(EnMsgType::ADD_GROUP_RESPONSE);
    response["errno"] = 1;
    response["errmsg"] = "You are already in this group!";
    conn->send(response.dump() + "\n");
    return;
  }
  // inGroupResult.status == NotFound → 继续

  // 检查群组是否存在
  BoolQueryResult groupExistResult = _groupModel.isGroupExist(groupid);
  if (groupExistResult.status == QueryStatus::DbError) {
    nlohmann::json response;
    response["msgid"] = static_cast<int>(EnMsgType::ADD_GROUP_RESPONSE);
    response["errno"] = 1;
    response["errmsg"] = "System busy, please try again later.";
    conn->send(response.dump() + "\n");
    return;
  }
  if (groupExistResult.status == QueryStatus::NotFound) {
    nlohmann::json response;
    response["msgid"] = static_cast<int>(EnMsgType::ADD_GROUP_RESPONSE);
    response["errno"] = 1;
    response["errmsg"] = "The group is not exist";
    conn->send(response.dump() + "\n");
    return;
  }
  // groupExistResult.status == Ok → 继续

  // 检查加群请求状态
  RequestStatusResult result =
      _groupRequestModel.queryRequestStatus(userid, groupid);
  if (result.status == QueryStatus::DbError) {
    nlohmann::json response;
    response["msgid"] = static_cast<int>(EnMsgType::ADD_GROUP_RESPONSE);
    response["errno"] = 1;
    response["errmsg"] = "System busy, please try again later.";
    conn->send(response.dump() + "\n");
    return;
  }
  if (result.status == QueryStatus::NotFound) {
    // 没有记录，插入新请求
    if (!_groupRequestModel.addGroupRequest(userid, groupid)) {
      nlohmann::json response;
      response["msgid"] = static_cast<int>(EnMsgType::ADD_GROUP_RESPONSE);
      response["errno"] = 3;
      response["errmsg"] = "System busy, please try again later.";
      conn->send(response.dump() + "\n");
      return;
    }
  } else {
    // QueryStatus::Ok — 请求记录已存在，根据状态决定是否允许重发
    if (result.value == "pending") {
      nlohmann::json response;
      response["msgid"] = static_cast<int>(EnMsgType::ADD_GROUP_RESPONSE);
      response["errno"] = 1;
      response["errmsg"] =
          "The request has been sent, please wait for approval!";
      conn->send(response.dump() + "\n");
      return;
    } else if (result.value == "rejected") {
      // rejected 状态可以再次申请，重置为 pending
      if (!_groupRequestModel.updateRequestStatus(userid, groupid, "pending")) {
        nlohmann::json response;
        response["msgid"] = static_cast<int>(EnMsgType::ADD_GROUP_RESPONSE);
        response["errno"] = 3;
        response["errmsg"] = "System busy, please try again later.";
        conn->send(response.dump() + "\n");
        return;
      }
    } else {
      LOG_ERROR("%s:%d: invalid GroupRequest status", __FILE_NAME__, __LINE__);
      return;
    }
  }

  // 获取群主 id 并将入群请求消息转发给群主
  IntQueryResult ownerResult = _groupModel.queryGroupOwnerId(groupid);
  if (ownerResult.status == QueryStatus::DbError) {
    nlohmann::json response;
    response["msgid"] = static_cast<int>(EnMsgType::ADD_GROUP_RESPONSE);
    response["errno"] = 3;
    response["errmsg"] = "System busy, please try again later.";
    conn->send(response.dump() + "\n");
    return;
  }
  if (ownerResult.status == QueryStatus::NotFound) {
    nlohmann::json response;
    response["msgid"] = static_cast<int>(EnMsgType::ADD_GROUP_RESPONSE);
    response["errno"] = 1;
    response["errmsg"] = "The group owner does not exist.";
    conn->send(response.dump() + "\n");
    return;
  }

  deliverMessage(ownerResult.value, js.dump());
}

// 处理加群申请(同意/拒绝)业务
void ChatService::addGroupHandle(const TcpConnectionPtr &conn,
                                 nlohmann::json &js, Timestamp time) {
  int userid = js["userid"].get<int>();
  int groupid = js["groupid"].get<int>();

  nlohmann::json response;
  // 判断当前的请求状态，如果不是pending状态，不允许同意或拒绝
  if (_groupRequestModel.isPendingRequest(userid, groupid)) {
    // 接受群主选择
    std::string action = js["action"];

    response["msgid"] = static_cast<int>(EnMsgType::ADD_GROUP_RESPONSE);

    if (action == "accept") {
      // 更新群组请求表
      _groupRequestModel.updateRequestStatus(userid, groupid, "accepted");
      // 加入群组
      _groupModel.addGroup(userid, groupid, "normal");
      response["errno"] = 0;
      response["errmsg"] =
          "Your request to join the group has been accepted. You can now chat!";
    } else {
      // 更新群组请求表
      _groupRequestModel.updateRequestStatus(userid, groupid, "rejected");
      response["errno"] = 1;
      response["errmsg"] =
          "Your request to join the group has been rejected. You can not chat!";
    }

    // 通知请求用户，群组申请结果
    deliverMessage(userid, response.dump());
  } else {
    // 此时反馈给用户，没有请求
    response["msgid"] = static_cast<int>(EnMsgType::ADD_GROUP_RESPONSE);
    response["errno"] = 2;
    response["errmsg"] = "You have no request to join the group!";
    conn->send(response.dump() + "\n");
  }
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
  std::string payload = js.dump();
  std::vector<TcpConnectionPtr> localConnections;
  std::vector<int> remoteUserIds;

  // 先在锁内做本地连接快照，避免持锁做 IO
  {
    std::lock_guard<std::mutex> lock(_connMutex);
    for (int id : useridVec) {
      auto it = _userConnMap.find(id);
      if (it != _userConnMap.end()) {
        localConnections.push_back(it->second);
      } else {
        remoteUserIds.push_back(id);
      }
    }
  }

  for (const TcpConnectionPtr &connection : localConnections) {
    connection->send(payload + "\n");
  }

  for (int id : remoteUserIds) {
    User user = _userModel.query(id);
    if (user.getState() == "online") {
      _redis.publish(id, payload);
    } else {
      _offlineMsgModel.insert(id, payload);
    }
  }
}

// 从redis消息队列中获取订阅消息
void ChatService::handleRedisSubscribeMessage(int userid, std::string msg) {
  LOG_INFO("dispatch redis subscribed message, userid=%d", userid);
  std::lock_guard<std::mutex> lock(_connMutex);

  auto it = _userConnMap.find(userid);
  if (it != _userConnMap.end()) {
    it->second->send(msg + "\n");
    return;
  }

  // 存储用户的离线消息
  _offlineMsgModel.insert(userid, msg);
}
