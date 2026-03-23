#ifndef FRIENDMODEL_H
#define FRIENDMODEL_H

#include "user.hpp"
#include <vector>

// 维护好友信息的接口操作方法
class FriendModel {
public:
  // 判断用户是否存在
  bool isUserExist(int friendid);

  // 判断是否已经是好友了（防止重复添加）
  bool isFriend(int userid, int friendid);

  // 添加好友关系
  void insert(int id, int friendid);

  // 返回用户的好友列表
  std::vector<User> query(int userid);
};

#endif