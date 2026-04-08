#ifndef REQUESTRESULT_H
#define REQUESTRESULT_H

#include <string>

// OK 表示查询成功 “pending” “accepted” “rejected”
// NotFound 表示没查询到结果 "" 还没有请求
// DbError 表示数据库错误
enum class QueryStatus { Ok, NotFound, DbError };

struct RequestStatusResult {
  // 表示查询的结果
  QueryStatus status;
  // value只有status == OK时才表示状态值 “pending” “accepted” “rejected”
  std::string value;
};

struct BoolQueryResult 
{
    QueryStatus status;
    bool value;
};

struct IntQueryResult
{
    QueryStatus status;
    int value;
};

#endif
