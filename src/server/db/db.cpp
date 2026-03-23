#include "../../../include/server/db/db.h"
#include <mymuduo/Logger.h>

// 数据库配置信息
static std::string server = "192.168.88.128";
static std::string user = "tpy";
static std::string password = "wang112233";
static std::string dbname = "ChatServer";

// 初始化数据库连接
MySQL::MySQL() { _conn = mysql_init(nullptr); }
// 释放数据库连接
MySQL::~MySQL() { mysql_close(_conn); }
// 连接数据库
bool MySQL::connect() {
  if (mysql_real_connect(_conn, server.c_str(), user.c_str(), password.c_str(),
                         dbname.c_str(), 3306, nullptr, 0) != nullptr) {
    // //C和C++代码默认的编码字符是ASCII,如果不设置,从MySQL上拉下来的中文显示?
    mysql_query(_conn, "set names gbk");
    LOG_INFO("connect mysql success!");
    return true;
  } else {
    LOG_INFO("connect mysql fail!");
    return false;
  }
}
// 更新操作
bool MySQL::update(std::string sql) {
  if (mysql_query(_conn, sql.c_str())) {
    // __FILE__代表当前文件名(字符串)，__LINE__代表当前行号(整数)
    LOG_INFO("%s:%d:%s更新失败！\n", __FILE__, __LINE__, sql.c_str());
    return false;
  }
  return true;
}

// 查询操作
MYSQL_RES *MySQL::query(std::string sql) {
  if (mysql_query(_conn, sql.c_str())) {
    LOG_INFO("%s:%d:%s查询失败！\n", __FILE__, __LINE__, sql.c_str());
    return nullptr;
  }
  return mysql_store_result(_conn);
}

// 获取连接
MYSQL *MySQL::getconnection() { return _conn; }