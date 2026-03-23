#include "../../../include/server/model/usermodel.hpp"
#include "../../../include/server/db/db.h"
#include <mysql/mysql.h>

// User表的增加方法
bool UserModel::insert(User &user) {
  // 1、组装sql语句
  char sql[1024] = {0};
  // sprintf 字符串连接
  sprintf(sql, "insert into User(name,password,state) values('%s','%s','%s')",
          user.getName().c_str(), user.getPassword().c_str(),
          user.getState().c_str());

  MySQL mysql;
  if (mysql.connect()) {
    if (mysql.update(sql)) {
      // 获取插入成功的用户数据生成的主键id
      user.setId(mysql_insert_id(mysql.getconnection()));
      return true;
    }
  }
  return false;
}

// User表的查询方法
User UserModel::query(
    std::string name) { // 1、组装sql语句，初始化一个长度为1024，数据全为0的数组
  char sql[1024] = {0};

  // sprintf 进行字符串连接
  std::sprintf(sql, "select * from User where name = '%s'", name.c_str());

  // 2、连接数据库，执行sql语句
  MySQL mysql;
  if (mysql.connect()) {
    MYSQL_RES *res = mysql.query(sql);
    if (res != nullptr) {
      // 获取查询结果
      MYSQL_ROW row = mysql_fetch_row(res);
      if (row != nullptr) {
        User user;
        user.setId(atoi(row[0]));
        user.setName(row[1]);
        user.setPassword(row[2]);
        user.setState(row[3]);

        mysql_free_result(res);
        return user;
      }
    }
  }
  return User();
}

// User表的查询方法
User UserModel::query(int id) {
  User user;
  // 1、组装sql语句，初始化一个长度为1024，数据全为0的数组
  char sql[1024] = {0};

  // sprintf 进行字符串连接
  std::sprintf(sql, "select * from User where id = '%d'", id);

  // 2、连接数据库，执行sql语句
  MySQL mysql;
  if (mysql.connect()) {
    MYSQL_RES *res = mysql.query(sql);
    if (res != nullptr) {
      // 获取查询结果
      MYSQL_ROW row = mysql_fetch_row(res);
      if (row != nullptr) {
        user.setId(atoi(row[0]));
        user.setName(row[1]);
        user.setState(row[3]);
      }
    }
    mysql_free_result(res);
  }
  return user;
}

// 更新用户状态信息
bool UserModel::updateState(User &user) {
  // 1、组装sql语句
  char sql[1024] = {0};

  // 字符串拼接
  std::sprintf(sql, "update User set state = '%s' where id = '%d'",
               user.getState().c_str(), user.getId());

  MySQL mysql;
  if (mysql.connect()) {
    if (mysql.update(sql)) {
      return true;
    }
  }
  return false;
}

// 重置用户的状态信息
void UserModel::resetState() {
  // 1、组装sql语句
  char sql[1024] = {0};

  // 字符串拼接
  std::sprintf(sql, "update User set state = 'offline' where state = 'online'");

  // 2、数据库连接
  MySQL mysql;
  if (mysql.connect()) {
    mysql.update(sql);
  }
}