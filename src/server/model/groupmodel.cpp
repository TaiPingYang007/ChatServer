#include "../../../include/server/model/groupmodel.hpp"
#include "../../../include/server/db/db.h"
#include "../../../include/server/logger.h"
#include <mysql/mysql.h>
#include <sstream>

// 判断群组是否已经存在
BoolQueryResult GroupModel::isGroupExist(std::string name) {
  std::ostringstream sql;
  sql << "select * from AllGroup where groupname = '" << name << "'";

  // 2、连接数据库
  MySQL mysql;
  if (mysql.connect()) {
    MYSQL_RES *res = mysql.query(sql.str());
    if (res != nullptr) {
      MYSQL_ROW row = mysql_fetch_row(res);
      if (row != nullptr && row[0] != nullptr) {
        mysql_free_result(res);
        return {QueryStatus::Ok, true};
      } else {
        mysql_free_result(res);
        return {QueryStatus::NotFound, false};
      }
    } else {
      LOG_ERROR("%s:%d: select AllGroup failed in isGroupExist(name=%s)",
                __FILE_NAME__, __LINE__, name.c_str());
      return {QueryStatus::DbError, false};
    }
  } else {
    LOG_ERROR("%s:%d: sql connect error!", __FILE_NAME__, __LINE__);
    return {QueryStatus::DbError, false};
  }
}

BoolQueryResult GroupModel::isGroupExist(int groupid) {
  std::ostringstream sql;
  sql << "select id from AllGroup where id = " << groupid;

  MySQL mysql;
  if (mysql.connect()) {
    MYSQL_RES *res = mysql.query(sql.str());
    if (res != nullptr) {
      MYSQL_ROW row = mysql_fetch_row(res);
      if (row != nullptr && row[0] != nullptr) {
        mysql_free_result(res);
        return {QueryStatus::Ok, true};
      } else {
        mysql_free_result(res);
        return {QueryStatus::NotFound, false};
      }
    } else {
      LOG_ERROR("%s:%d: select AllGroup failed in isGroupExist(id=%d)",
                __FILE_NAME__, __LINE__, groupid);
      return {QueryStatus::DbError, false};
    }
  } else {
    LOG_ERROR("%s:%d: sql connect error!", __FILE_NAME__, __LINE__);
    return {QueryStatus::DbError, false};
  }
}

// 创建群组
bool GroupModel::createGroup(Group &group) {
  std::ostringstream sql;
  sql << "insert into AllGroup (groupname , groupdesc) value ('"
      << group.getName() << "','" << group.getDesc() << "')";
  // 2、连接数据库
  MySQL mysql;
  if (mysql.connect()) {
    if (mysql.update(sql.str())) {
      group.setId(mysql_insert_id(mysql.getconnection()));
      return true;
    }
  }
  return false;
}

// 加入群组
void GroupModel::addGroup(int userid, int groupid, std::string role) {
  std::ostringstream sql;
  sql << "insert into GroupUser value (" << groupid << "," << userid << ",'"
      << role << "')";
  // 2、连接数据库
  MySQL mysql;
  if (mysql.connect()) {
    mysql.update(sql.str());
  }
}

// 判断是不是已经在群组中
BoolQueryResult GroupModel::isUserInGroup(int userid, int groupid) {
  std::ostringstream sql;
  sql << "select * from GroupUser where userid = '" << userid
      << "' and groupid = '" << groupid << "'";

  // 2、连接数据库
  MySQL mysql;
  if (mysql.connect()) {
    MYSQL_RES *res = mysql.query(sql.str());
    if (res != nullptr) {
      MYSQL_ROW row = mysql_fetch_row(res);
      if (row != nullptr && row[0] != nullptr) {
        mysql_free_result(res);
        return {QueryStatus::Ok, true};
      } else {
        mysql_free_result(res);
        return {QueryStatus::NotFound, false};
      }
    } else {
      LOG_ERROR("%s:%d: select GroupUser failed in isUserInGroup(userid=%d, groupid=%d)",
                __FILE_NAME__, __LINE__, userid, groupid);
      return {QueryStatus::DbError, false};
    }
  } else {
    LOG_ERROR("%s:%d: sql connect error!", __FILE_NAME__, __LINE__);
    return {QueryStatus::DbError, false};
  }
}

// 获取群主id
IntQueryResult GroupModel::queryGroupOwnerId(int groupid) {
  std::ostringstream sql;
  sql << "select userid from GroupUser where groupid = " << groupid
      << " and grouprole = 'creator'";

  MySQL mysql;
  if (!mysql.connect()) {
    return {QueryStatus::DbError, -1};
  }

  MYSQL_RES *res = mysql.query(sql.str());
  if (res == nullptr) {
    return {QueryStatus::DbError, -1};
  }

  MYSQL_ROW row = mysql_fetch_row(res);
  if (row == nullptr || row[0] == nullptr) {
    mysql_free_result(res);
    return {QueryStatus::NotFound, -1};
  }

  int id = atoi(row[0]);
  mysql_free_result(res);
  return {QueryStatus::Ok, id};
}

// 获取群组名
std::string GroupModel::queryGroupName(int groupid) {
  std::string groupname = "";
  std::ostringstream sql;
  sql << "select groupname from AllGroup where id = " << groupid;

  // 2、连接数据库
  MySQL mysql;
  if (mysql.connect()) {
    MYSQL_RES *res = mysql.query(sql.str());
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
  std::ostringstream sql;
  sql << "select a.id,a.groupname,a.groupdesc from AllGroup a inner join "
         "GroupUser b on a.id = b.groupid where b.userid = "
      << userid;

  // 2、创建组容器
  std::vector<Group> groupVec;
  // 3、连接数据库
  MySQL mysql;
  if (mysql.connect()) {
    MYSQL_RES *res = mysql.query(sql.str());
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
    std::ostringstream memberSql;
    memberSql << "select a.id,a.name,a.state,b.grouprole from User a inner join "
                 "GroupUser b on a.id = b.userid where b.groupid = "
              << group.getId();
    MySQL mysql;
    if (mysql.connect()) {
      MYSQL_RES *res = mysql.query(memberSql.str());
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
  std::ostringstream sql;
  sql << "select userid from GroupUser where groupid = " << groupid
      << " and userid != " << userid;
  // 2、创建id容器
  std::vector<int> idVec;
  // 3、连接数据库
  MySQL mysql;
  if (mysql.connect()) {
    MYSQL_RES *res = mysql.query(sql.str());
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
