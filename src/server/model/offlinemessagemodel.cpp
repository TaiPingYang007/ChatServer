#include "../../../include/server/model/offlinemessagemodel.hpp"
#include "../../../include/server/db/db.h"

// 存储用户的离线消息
void OfflineMsgModel::insert(int userid, std::string msg) {
  // 1、组装sql语句
  char sql[1024] = {0};
  sprintf(sql, "insert into OfflineMessage value ('%d','%s')", userid,
          msg.c_str());
  // 2、连接数据库
  MySQL mysql;
  if (mysql.connect()) {
    mysql.update(sql);
  }
}

// 删除用户的离线消息
void OfflineMsgModel::remove(int userid) {
  // 1、组装sql语句
  char sql[1024] = {0};
  sprintf(sql, "delete from OfflineMessage where userid = '%d'", userid);
  // 2、连接数据库
  MySQL mysql;
  if (mysql.connect()) {
    mysql.update(sql);
  }
}

// 查询用户的离线消息
std::vector<std::string> OfflineMsgModel::query(int userid) {
  // 1、组装sql语句
  char sql[1024] = {0};
  sprintf(sql, "select message from OfflineMessage where userid = '%d'",
          userid);

  // 2、连接数据库
  std::vector<std::string> vec;
  MySQL mysql;
  if (mysql.connect()) {
    MYSQL_RES *res = mysql.query(sql);
    if (res != nullptr) {
      MYSQL_ROW row;
      while ((row = mysql_fetch_row(res))) {
        // 把userid用户的所有的离线消息放入vec中返回
        vec.emplace_back(row[0]);
      }
      mysql_free_result(res);
    }
  }
  return vec;
}