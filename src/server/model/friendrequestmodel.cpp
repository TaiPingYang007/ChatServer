#include "../../../include/server/model/friendrequestmodel.hpp"
#include "../../../include/server/db/db.h"
#include "../../../include/server/logger.h"
#include <iostream>
#include <mysql/mysql.h>
#include <sstream>

// 添加好友请求操作
bool FriendRequestModel::addFriendRequest(int userid, int targetid) {
  // 1、组装sql语句
  std::ostringstream sql;
  sql << "insert into FriendRequest (requester_id, target_id,status) values ("
      << userid << "," << targetid << "," << "'pending');";

  // 2、连接数据库
  MySQL mysql;

  // 判断是否连接成功
  if (mysql.connect()) {
    // 执行sql语句
    if (mysql.update(sql.str())) {
      return true;
    } else {
      LOG_ERROR("%s:%d: insert FriendRequest failed error sql: %s",
                __FILE_NAME__, __LINE__, sql.str().c_str());
      return false;
    }
  } else {
    LOG_ERROR("%s:%d: sql connect error!", __FILE_NAME__, __LINE__);
    return false;
  }
}

// 获取好友请求状态 userid targetid
/*
    0：accpetfriend:1
    判断 1 是不是真的向 0 发送了好友请求
*/
BoolQueryResult FriendRequestModel::isPendingRequest(int userid, int targetid) {
  // 1、组装sql语句
  std::ostringstream sql;
  sql << "select status from FriendRequest where requester_id = " << userid
      << " and target_id = " << targetid << ";";

  // 2、连接数据库
  MySQL mysql;
  if (mysql.connect()) {
    // 执行sql语句
    MYSQL_RES *res = mysql.query(sql.str());
    if (res != nullptr) {
      // sql语句执行成功，查询成功
      MYSQL_ROW row = mysql_fetch_row(res);
      if (row != nullptr && std::string(row[0]) == "pending") {
        mysql_free_result(res);
        return {QueryStatus::Ok, true};
      } else {
        mysql_free_result(res);
        return {QueryStatus::NotFound, false};
      }
    } else {
      // sql语句执行失败，查询失败
      LOG_ERROR("%s:%d: select FriendRequest failed error sql: %s",
                __FILE_NAME__, __LINE__, sql.str().c_str());
      // 这里res已经是nullptr了不需要再执行mysql_free_result(res);
      return {QueryStatus::DbError, false};
    }
  } else {
    LOG_ERROR("%s:%d: sql connect error!", __FILE_NAME__, __LINE__);
    return {QueryStatus::DbError, false};
  }
}

// 写入请求结果和处理事件
bool FriendRequestModel::updateRequestStatus(int userid, int targetid,
                                             std::string result) {
  // 1、组装sql语句
  std::ostringstream sql;
  if (result == "accepted" || result == "rejected") {
    sql << "update FriendRequest set status = '" << result
        << "', handled_at = now() where requester_id = " << userid
        << " and target_id = " << targetid << ";";
  } else if (result == "pending") {
    sql << "update FriendRequest set status = '" << result
        << "', handled_at = null where requester_id = " << userid
        << " and target_id = " << targetid << ";";
  } else {
    LOG_ERROR("%s:%d: invalid FriendRequest status: %s", __FILE_NAME__,
              __LINE__, result.c_str());
    return false;
  }

  // 2、连接数据库
  MySQL mysql;
  if (mysql.connect()) {
    // 执行sql语句
    if (mysql.update(sql.str())) {
      return true;
    } else {
      LOG_ERROR("%s:%d: insert FriendRequest failed error sql: %s",
                __FILE_NAME__, __LINE__, sql.str().c_str());
      return false;
    }
  } else {
    LOG_ERROR("%s:%d: sql connect error!", __FILE_NAME__, __LINE__);
    return false;
  }
}

// 查询请求状态
RequestStatusResult FriendRequestModel::queryRequestStatus(int userid,
                                                           int targetid) {
  std::ostringstream sql;
  sql << "select status from FriendRequest where requester_id = " << userid
      << " and target_id = " << targetid << ";";

  MySQL mysql;
  if (!mysql.connect()) {
    LOG_ERROR("%s:%d: sql connect error!", __FILE_NAME__, __LINE__);
    return {QueryStatus::DbError, ""};
  }

  MYSQL_RES *res = mysql.query(sql.str());
  if (res == nullptr) {
    LOG_ERROR("%s:%d: select FriendRequest failed error sql: %s",
              __FILE_NAME__, __LINE__, sql.str().c_str());
    return {QueryStatus::DbError, ""};
  }

  MYSQL_ROW row = mysql_fetch_row(res);
  if (row == nullptr || row[0] == nullptr) {
    mysql_free_result(res);
    return {QueryStatus::NotFound, ""};
  }

  std::string status = row[0];
  mysql_free_result(res);
  return {QueryStatus::Ok, status};
}
