#include "../../../include/server/model/grouprequestmodel.hpp"
#include "../../../include/server/db/db.h"
#include "../../../include/server/logger.h"
#include <mysql/mysql.h>
#include <sstream>

// 向群组请求表添加群组请求
bool GroupRequestModel::addGroupRequest(int userid, int groupid) {
  // 1、组装sql语句
  std::ostringstream sql;
  sql << "insert into GroupRequest (requester_id, group_id, status) values ("
      << userid << "," << groupid << ", 'pending');";

  // 2、连接数据库
  MySQL mysql;
  if (mysql.connect()) {
    // 数据库连接成功
    if (mysql.update(sql.str())) {
      // 更新成功
      return true;
    } else {
      // 更新失败写入日志
      LOG_ERROR("%s:%d: insert GroupRequest failed error sql: %s",
                __FILE_NAME__, __LINE__, sql.str().c_str());
      return false;
    }
  } else {
    // 数据库连接失败
    LOG_ERROR("%s:%d: sql connect error!", __FILE_NAME__, __LINE__);
    return false;
  }
}

// 判断群组请求状态是不是pending
bool GroupRequestModel::isPendingRequest(int userid, int groupid) {
  // 1、组装sql语句
  std::ostringstream sql;
  sql << "select status from GroupRequest where requester_id = " << userid
      << " and group_id = " << groupid << ";";

  // 2、连接数据库
  MySQL mysql;
  if (mysql.connect()) {
    // 数据库连接成功
    // 执行sql语句
    MYSQL_RES *res = mysql.query(sql.str());
    if (res != nullptr) {
      // sql语句执行成功。查询成功
      MYSQL_ROW row = mysql_fetch_row(res);
      if (row != nullptr && std::string(row[0]) == "pending") {
        mysql_free_result(res);
        return true;
      } else {
        mysql_free_result(res);
        return false;
      }
    } else {
      // sql语句执行失败，查询失败
      LOG_ERROR("%s:%d: select GroupRequest failed error sql: %s",
                __FILE_NAME__, __LINE__, sql.str().c_str());
      // 这里res已经是nullptr了，不需要再执行mysql_free_result(res);
      return false;
    }
  } else {
    // 数据库连接失败
    LOG_ERROR("%s:%d: sql connect error!", __FILE_NAME__, __LINE__);
    return false;
  }
}
// 更新群组请求状态
bool GroupRequestModel::updateRequestStatus(int userid, int groupid,
                                            std::string result) {
  // 1、组装sql语句
  std::ostringstream sql;
  // 如果是更新为accepted和rejected，需要修改时间
  if (result == "accepted" || result == "rejected") {
    sql << "update GroupRequest set status = '" << result
        << "', handled_at = now() where requester_id = " << userid
        << " and group_id = " << groupid << ";";
  } else if (result == "pending") {

    sql << "update GroupRequest set status = '" << result
        << "', handled_at = null where requester_id = " << userid
        << " and group_id = " << groupid << ";";
  } else {
    LOG_ERROR("%s:%d: invalid GroupRequest status: %s", __FILE_NAME__,
              __LINE__, result.c_str());
    return false;
  }

  // 2、连接数据库
  MySQL mysql;
  if (mysql.connect()) {
    // 数据库连接成功
    // 执行sql语句
    if (mysql.update(sql.str())) {
      // 更新成功
      return true;
    } else {
      // 更新失败
      LOG_ERROR("%s:%d: update GroupRequest failed error sql: %s",
                __FILE_NAME__, __LINE__, sql.str().c_str());
      return false;
    }
  } else {
    // 数据库连接失败
    LOG_ERROR("%s:%d: sql connect error!", __FILE_NAME__, __LINE__);
    return false;
  }
}
// 查询群组请求状态
RequestStatusResult GroupRequestModel::queryRequestStatus(int userid, int groupid) {
  std::string result;
  // 1、组装sql语句
  std::ostringstream sql;
  sql << "select status from GroupRequest where requester_id = " << userid
      << " and group_id = " << groupid << ";";

  // 2、连接数据库
  MySQL mysql;
  if (mysql.connect()) {
    // 数据库连连接成功
    // 执行查询语句
    MYSQL_RES *res = mysql.query(sql.str());
    if (res != nullptr) {
      MYSQL_ROW row = mysql_fetch_row(res);
      if (row != nullptr && row[0] != nullptr) {
        result = std::string(row[0]);
        mysql_free_result(res);
        return {QueryStatus::Ok, result};
      } else {
        mysql_free_result(res);
        return {QueryStatus::NotFound, ""};
      }
    } else {
      // sql语句执行失败，查询失败
      LOG_ERROR("%s:%d: select GroupRequest failed error sql: %s",
                __FILE_NAME__, __LINE__, sql.str().c_str());
      return {QueryStatus::DbError, ""};
    }
  } else {
    // 数据库连接失败
    LOG_ERROR("%s:%d: sql connect failed!", __FILE_NAME__, __LINE__);
    return {QueryStatus::DbError, ""};
  }
}
