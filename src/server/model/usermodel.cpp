#include "../../../include/server/model/usermodel.hpp"
#include "../../../include/server/db/db.h"
#include <mysql/mysql.h>
#include <sstream>

// User表的增加方法
bool UserModel::insert(User &user) {
  std::ostringstream sql;
  sql << "insert into User(name,password,state) values('"
      << user.getName() << "','" << user.getPassword() << "','"
      << user.getState() << "')";

  MySQL mysql;
  if (mysql.connect()) {
    if (mysql.update(sql.str())) {
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
  std::ostringstream sql;
  sql << "select * from User where name = '" << name << "'";

  // 2、连接数据库，执行sql语句
  MySQL mysql;
  if (mysql.connect()) {
    MYSQL_RES *res = mysql.query(sql.str());
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
      mysql_free_result(res);
    }
  }
  return User();
}

// User表的查询方法
User UserModel::query(int id) {
  User user;
  std::ostringstream sql;
  sql << "select * from User where id = '" << id << "'";

  // 2、连接数据库，执行sql语句
  MySQL mysql;
  if (mysql.connect()) {
    MYSQL_RES *res = mysql.query(sql.str());
    if (res != nullptr) {
      // 获取查询结果
      MYSQL_ROW row = mysql_fetch_row(res);
      if (row != nullptr) {
        user.setId(atoi(row[0]));
        user.setName(row[1]);
        // row[2] = password 故意跳过
        user.setState(row[3]);
      }
    }
    mysql_free_result(res);
  }
  return user;
}

// 更新用户状态信息
bool UserModel::updateState(User &user) {
  std::ostringstream sql;
  sql << "update User set state = '" << user.getState()
      << "' where id = '" << user.getId() << "'";

  MySQL mysql;
  if (mysql.connect()) {
    if (mysql.update(sql.str())) {
      return true;
    }
  }
  return false;
}

// 重置用户的状态信息
void UserModel::resetState() {
  std::ostringstream sql;
  sql << "update User set state = 'offline' where state = 'online'";

  // 2、数据库连接
  MySQL mysql;
  if (mysql.connect()) {
    mysql.update(sql.str());
  }
}
