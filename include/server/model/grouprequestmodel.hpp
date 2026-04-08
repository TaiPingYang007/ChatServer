#ifndef GROUPREQUESTMODEL_H
#define GROUPREQUESTMODEL_H

#include <string>
#include "./requestresult.hpp"

class GroupRequestModel {
public:
  // 向群组请求表添加群组请求
  bool addGroupRequest(int userid, int groupid);
  // 判断群组请求状态是不是pending
  bool isPendingRequest(int userid, int groupid);
  // 更新群组请求状态
  bool updateRequestStatus(int userid, int groupid, std::string result);
  // 查询群组请求状态
  RequestStatusResult queryRequestStatus(int userid, int groupid);
};

#endif