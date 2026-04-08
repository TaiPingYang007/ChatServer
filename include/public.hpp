#ifndef PUBLIC_H
#define PUBLIC_H
/*
server和client的公共文件
*/
#include <cstdint>

enum class EnMsgType : uint8_t {
  LOGIN_MSG = 1,    // 登录消息
  LOG_MSG_ACK = 2,  // 登录响应消息
  LOGINOUT_MSG = 3, // 退出登录

  REG_MSG = 4,     // 注册消息
  REG_MSG_ACK = 5, // 注册响应消息

  ONE_CHAT_MSG = 6,     // 私聊消息
  ONE_CHAT_MSG_ACK = 7, // 私聊响应消息

  ADD_FRIEND_REQUEST = 8, // 发起添加好友请求 (A -> Server, Server -> B)
  ADD_FRIEND_HANDLE = 9,  // 处理好友申请(同意/拒绝)反馈给服务器
  ADD_FRIEND_RESPONSE = 10, // 服务器统一返回给A的响应结果

  CREATE_GROUP_MSG = 11,     // 创建群组消息
  CREATE_GROUP_MSG_ACK = 12, // 群组响应消息
  ADD_GROUP_MSG = 13,        // 加入群组消息
  ADD_GROUP_HANDLE = 14, // 处理加群申请(同意/拒绝)反馈给服务器
  ADD_GROUP_RESPONSE = 15, // 服务器统一返回给请求加群用户的响应结果
  GROUP_CHAT_MSG = 16, // 群聊消息
};

#endif