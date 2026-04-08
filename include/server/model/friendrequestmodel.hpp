// 好友请求模型
#ifndef FRIENDREQUESTMODEL_H
#define FRIENDREQUESTMODEL_H
#include "./requestresult.hpp"
#include <string>

class FriendRequestModel {
public:
    // 添加好友请求操作
    bool addFriendRequest (int userid , int targetid);

    // 获取好友请求状态 userid targetid
    /*
        0：accpetfriend:1
        判断 1 是不是真的向 0 发送了好友请求
    */
    BoolQueryResult isPendingRequest(int userid, int targetid);

    // 写入请求结果
    bool updateRequestStatus(int userid, int targetid, std::string result);

    // 查询请求状态
    RequestStatusResult queryRequestStatus(int userid, int targetid);
};

#endif