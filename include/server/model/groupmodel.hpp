#ifndef GROUPMODEL_H
#define GROUPMODEL_H

#include "group.hpp"

// 维护群组信息的操作接口方法
class GroupModel {
public:
  // 创建群组
  bool createGroup(Group &group);

  // 加入群组
  void addGroup(int userid, int groupid, std::string role);

  // 获取群主id
  int queryGroupOwnerId(int groupid);

  // 获取群组名
  std::string queryGroupName(int groupid);

  // 查询用户所在群组信息
  std::vector<Group> queryGroup(int userid);

  // 根据指定的groupid查询群组用户id列表，除userid自己，主要用户群聊业务给群组其他成员群发消息
  std::vector<int> queryGroupUsers(int userid, int groupid);
};

#endif