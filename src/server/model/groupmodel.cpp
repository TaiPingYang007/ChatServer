#include "../../../include/server/model/groupmodel.hpp"
#include "../../../include/server/db/db.h"
#include <mysql/mysql.h>

// 创建群组
bool GroupModel::createGroup(Group &group) {
  // 1、组装sql语句
  char sql[1024] = {0};
  std::sprintf(sql,
               "insert into AllGroup (groupname , groupdesc) value ('%s','%s')",
               group.getName().c_str(), group.getDesc().c_str());
  // 2、连接数据库
  MySQL mysql;
  if (mysql.connect()) {
    if (mysql.update(sql)) {
      group.setId(mysql_insert_id(mysql.getconnection()));
      return true;
    }
  }
  return false;
}

// 加入群组
void GroupModel::addGroup(int userid, int groupid, std::string role) {
  // 1、组装sql语句
  char sql[1024] = {0};
  std::sprintf(sql, "insert into GroupUser value (%d,%d,'%s')", groupid, userid,
               role.c_str());
  // 2、连接数据库
  MySQL mysql;
  if (mysql.connect()) {
    mysql.update(sql);
  }
}

// 获取群主id
int GroupModel::queryGroupOwnerId(int groupid) {
  // 1、组装sql语句
  char sql[1024] = {0};

  std::sprintf(
      sql,
      "select userid from GroupUser where groupid = %d and grouprole = '%s'",
      groupid, "creator");

  // 2、连接数据库
  MySQL mysql;
  if (mysql.connect()) {
    MYSQL_RES *res = mysql.query(sql);
    if (res != nullptr) {
      MYSQL_ROW row = mysql_fetch_row(res);
      if (row != nullptr) {
        int id = atoi(row[0]);
        mysql_free_result(res);
        return id;
      }
      mysql_free_result(res);
    }
  }
  return -1;
}

// 获取群组名
std::string GroupModel::queryGroupName(int groupid) {
  std::string groupname = "";
  // 1、组装sql语句
  char sql[1024] = {0};
  sprintf(sql, "select groupname from AllGroup where groupid = %d", groupid);

  // 2、连接数据库
  MySQL mysql;
  if (mysql.connect()) {
    MYSQL_RES *res = mysql.query(sql);
    if (res != nullptr) {
      MYSQL_ROW row = mysql_fetch_row(res);
      if (row != nullptr) {
        groupname = row[0];
      }
      mysql_free_result(res);
    }
  }
  return groupname;
}
// 查询用户所在群组信息
std::vector<Group> GroupModel::queryGroup(int userid) {
  // 1、组装sql语句
  char sql[1024] = {0};
  std::sprintf(sql,
               "select a.id,a.groupname,a.groupdesc from AllGroup a inner join "
               "GroupUser b on a.id = b.groupid where b.userid = %d",
               userid);

  // 2、创建组容器
  std::vector<Group> groupVec;
  // 3、连接数据库
  MySQL mysql;
  if (mysql.connect()) {
    MYSQL_RES *res = mysql.query(sql);
    if (res != nullptr) {
      MYSQL_ROW row;
      while ((row = mysql_fetch_row(res)) != nullptr) {
        Group group;
        group.setId(atoi(row[0]));
        group.setName(row[1]);
        group.setDesc(row[2]);
        groupVec.emplace_back(group);
      }
      mysql_free_result(res);
    }
  }

  // 查询群组的组员信息
  for (Group &group : groupVec) {
    sprintf(sql,
            "select a.id,a.name,a.state,b.role from User a inner join "
            "GroupUser b on a.id = b.userid where b.groupid = %d",
            group.getId());
    MySQL mysql;
    if (mysql.connect()) {
      MYSQL_RES *res = mysql.query(sql);
      if (res != nullptr) {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res)) != nullptr) {
          GroupUser user;
          user.setId(atoi(row[0]));
          user.setName(row[1]);
          user.setState(row[2]);
          user.setRole(row[3]);
          group.getUsers().emplace_back(user);
        }
        mysql_free_result(res);
      }
    }
  }
  return groupVec;
}

// 根据指定的groupid查询群组用户id列表，除userid自己，主要用户群聊业务给群组其他成员群发消息
std::vector<int> GroupModel::queryGroupUsers(int userid, int groupid) {
  // 1、组装sql语句
  char sql[1024] = {0};
  std::sprintf(
      sql, "select userid from GroupUser where groupid = %d and userid != %d",
      groupid, userid);
  // 2、创建id容器
  std::vector<int> idVec;
  // 3、连接数据库
  MySQL mysql;
  if (mysql.connect()) {
    MYSQL_RES *res = mysql.query(sql);
    if (res != nullptr) {
      MYSQL_ROW row;
      while ((row = mysql_fetch_row(res)) != nullptr) {
        idVec.emplace_back(atoi(row[0]));
      }
      mysql_free_result(res);
    }
  }
  return idVec;
}