#include "../../../include/server/model/friendmodel.hpp"
#include "../../../include/server/db/db.h"
#include <mysql/mysql.h>
#include <sstream>

// 判断好友是否存在
bool FriendModel::isUserExist(int friendid) {
  std::ostringstream sql;
  sql << "select * from User where id = " << friendid;

  // 2、连接数据库
  MySQL mysql;
  if (mysql.connect()) {
    MYSQL_RES *res = mysql.query(sql.str());
    if (res != nullptr) {
      MYSQL_ROW row = mysql_fetch_row(res);
      if (row != nullptr) {
        return true;
      }
    }
    mysql_free_result(res);
  }
  return false;
}

// 判断是否已经是好友了（防止重复添加）
bool FriendModel::isFriend(int userid, int friendid) {
  std::ostringstream sql;
  sql << "select * from Friend where userid = " << userid
      << " and friendid = " << friendid;

  // 2、连接数据库
  MySQL mysql;
  if (mysql.connect()) {
    MYSQL_RES *res = mysql.query(sql.str());
    if (res != nullptr) {
      MYSQL_ROW row = mysql_fetch_row(res);
      if (row != nullptr) {
        mysql_free_result(res);
        return true;
      }
      mysql_free_result(res);
    }
  }
  return false;
}

// 添加好友关系
void FriendModel::insert(int id, int friendid) {
  std::ostringstream sql;
  sql << "insert into Friend value ('" << id << "','" << friendid << "')";

  // 2、执行sql语句
  MySQL mysql;
  if (mysql.connect()) {
    mysql.update(sql.str());
  }
}

// 返回用户的好友列表
std::vector<User> FriendModel::query(int userid) {
  std::vector<User> vec;

  std::ostringstream sql;
  sql << "select User.id,User.name,User.state from User inner join "
         "Friend on User.id = Friend.friendid where Friend.userid = "
      << userid;

  // 2、执行sql语句
  MySQL mysql;
  if (mysql.connect()) {
    MYSQL_RES *res = mysql.query(sql.str());
    if (res != nullptr) {
      // 遍历结果集
      MYSQL_ROW row;
      while ((row = mysql_fetch_row(res)) != nullptr) {
        User user;
        user.setId(atoi(row[0]));
        user.setName(row[1]);
        user.setState(row[2]);
        vec.emplace_back(user);
      }
    }
    mysql_free_result(res);
  }
  return vec;
}
