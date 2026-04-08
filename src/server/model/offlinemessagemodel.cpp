#include "../../../include/server/model/offlinemessagemodel.hpp"
#include "../../../include/server/db/db.h"
#include <sstream>

// 存储用户的离线消息
void OfflineMsgModel::insert(int userid, std::string msg) {
  std::ostringstream sql;
  sql << "insert into OfflineMessage value ('" << userid << "','" << msg
      << "')";
  // 2、连接数据库
  MySQL mysql;
  if (mysql.connect()) {
    mysql.update(sql.str());
  }
}

// 删除用户的离线消息
void OfflineMsgModel::remove(int userid) {
  std::ostringstream sql;
  sql << "delete from OfflineMessage where userid = '" << userid << "'";
  // 2、连接数据库
  MySQL mysql;
  if (mysql.connect()) {
    mysql.update(sql.str());
  }
}

// 查询用户的离线消息
std::vector<std::string> OfflineMsgModel::query(int userid) {
  std::ostringstream sql;
  sql << "select message from OfflineMessage where userid = '" << userid
      << "'";

  // 2、连接数据库
  std::vector<std::string> vec;
  MySQL mysql;
  if (mysql.connect()) {
    MYSQL_RES *res = mysql.query(sql.str());
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
