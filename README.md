# C++ 集群聊天服务器

> 基于 Muduo + Redis + Nginx 的高并发分布式即时通讯系统

---

## 项目简介

本项目是一个从零开发的**高并发、可横向扩展**的即时聊天后端服务。核心挑战在于：单台服务器连接数有上限，而分布式部署后，连接到不同节点的用户之间如何实时通信？

**解决思路：**

```
Client A          Client B
   |                 |
[ChatServer-1]   [ChatServer-2]
   |                 |
   +----> Redis <----+   ← 跨节点消息总线
              |
           MySQL        ← 离线消息 / 持久化存储
```

1. **Nginx TCP 负载均衡**：统一入口，将客户端连接轮询分发到各 ChatServer 节点
2. **Redis Pub/Sub 消息总线**：A 在节点1，B 在节点2，消息经 Redis 频道中转，实现跨节点实时投递
3. **三级路由策略**：本地连接直投 → Redis 跨节点转发 → MySQL 离线存储，零消息丢失

---

## 技术栈

| 层次 | 技术 | 说明 |
|------|------|------|
| 网络层 | Muduo | 多线程 Reactor 模型，基于 epoll 的事件驱动 |
| 协议层 | nlohmann/json | 轻量 JSON 序列化，自定义应用层消息协议 |
| 存储层 | MySQL | 用户信息、好友关系、群组、离线消息持久化 |
| 消息中间件 | Redis (hiredis) | Pub/Sub 实现跨服节点实时消息路由 |
| 负载均衡 | Nginx stream | 四层 TCP 负载均衡，对用户无感知 |
| 构建 | CMake + Shell | 外部构建 + 一键编译脚本 |

---

## 核心功能

- **账号系统**：注册、登录、登出，防重复登录
- **好友管理**：发送好友申请 → 对方实时收到请求 → 同意/拒绝
- **群组管理**：创建群组、申请入群 → 群主审批 → 同意/拒绝
- **即时消息**：单聊、群聊，在线实时投递，离线自动存储并在下次登录时推送
- **跨节点通信**：连接在不同服务器节点的用户之间消息无缝互通

---

## 关键设计细节

### 1. 多线程安全的消息分发

用户连接表 `_userConnMap` 由多个 IO 线程并发读写，采用 `std::mutex` 保护。群聊时先在锁内**快照**成员列表，再在锁外逐一发送，避免持锁期间 IO 阻塞造成锁竞争：

```cpp
// 锁内仅做快照，避免持锁 IO
std::vector<int> memberIds;
{
    std::lock_guard<std::mutex> lock(_connMutex);
    for (auto &member : members)
        memberIds.push_back(member.getId());
}
// 锁外逐一投递
for (int id : memberIds)
    deliverMessage(id, payload);
```

### 2. Redis 跨节点路由

每个用户登录时订阅以自己 `userid` 为名的 Redis 频道，退出时取消订阅。发送方将消息 `PUBLISH` 到目标用户的频道，接收方节点的订阅回调将消息写入对应 TCP 连接：

```
发送方节点: PUBLISH <targetId> <message>
接收方节点: subscribe回调 → 找到本地连接 → send()
```

### 3. 异步日志系统

基于**无锁队列 + 独立日志线程**的异步日志，IO 线程仅入队不阻塞，日志线程消费队列写文件，避免磁盘 IO 拖慢业务处理：

```
IO线程: LOG_INFO(...) → 入队(无锁) → 立即返回
日志线程: 出队 → 写文件
```

---

## 目录结构

```
ChatServer/
├── include/
│   ├── public.hpp                  # 消息类型枚举（应用层协议）
│   └── server/
│       ├── chatservice.hpp         # 业务层核心类
│       ├── chatserver.hpp          # 网络层：连接/消息回调
│       ├── logger.h                # 异步日志
│       ├── lockqueue.h             # 无锁队列（日志用）
│       ├── redis/redis.hpp         # Redis Pub/Sub 封装
│       ├── db/db.h                 # MySQL 连接封装
│       └── model/                  # 数据模型层（ORM）
├── src/
│   ├── server/                     # 服务端实现
│   └── client/                     # 命令行客户端实现
├── bin/                            # 编译产物
├── autobuild.sh                    # 一键构建脚本
└── CMakeLists.txt
```

---

## 环境依赖

| 依赖 | 版本要求 | 安装参考 |
|------|---------|---------|
| GCC/G++ | >= C++11 | `apt install g++` |
| CMake | >= 3.10 | `apt install cmake` |
| Muduo | 任意稳定版 | 源码编译安装 |
| MySQL | >= 5.7 | `apt install libmysqlclient-dev` |
| Redis + hiredis | 任意稳定版 | `apt install redis-server libhiredis-dev` |
| Nginx | 需 stream 模块 | 源码编译：`--with-stream` |

---

## 快速开始

### 编译

```bash
git clone https://github.com/TaiPingYang007/ChatServer.git
cd ChatServer
chmod +x autobuild.sh
./autobuild.sh
# 产物：bin/ChatServer  bin/ChatClient
```

### 数据库初始化

```sql
CREATE DATABASE chat;
USE chat;

CREATE TABLE user (
    id INT PRIMARY KEY AUTO_INCREMENT,
    name VARCHAR(50) NOT NULL UNIQUE,
    password VARCHAR(50) NOT NULL,
    state ENUM('online','offline') DEFAULT 'offline'
);

CREATE TABLE friend (
    userid INT NOT NULL,
    friendid INT NOT NULL,
    PRIMARY KEY(userid, friendid)
);

CREATE TABLE allgroup (
    id INT PRIMARY KEY AUTO_INCREMENT,
    groupname VARCHAR(50) NOT NULL UNIQUE,
    groupdesc VARCHAR(200) DEFAULT ''
);

CREATE TABLE groupuser (
    groupid INT NOT NULL,
    userid INT NOT NULL,
    grouprole ENUM('creator','normal') DEFAULT 'normal',
    PRIMARY KEY(groupid, userid)
);

CREATE TABLE offlinemessage (
    userid INT NOT NULL,
    message VARCHAR(500) NOT NULL
);

CREATE TABLE friendrequest (
    id INT PRIMARY KEY AUTO_INCREMENT,
    fromid INT NOT NULL,
    toid INT NOT NULL,
    status ENUM('pending','accepted','rejected') DEFAULT 'pending'
);
```

### 配置 Nginx TCP 负载均衡

在 `nginx.conf` 中添加（`http {}` 块同级）：

```nginx
stream {
    upstream ChatServer {
        server 127.0.0.1:6000 weight=1;
        server 127.0.0.1:6002 weight=1;
    }
    server {
        listen 8000;
        proxy_pass ChatServer;
    }
}
```

### 启动集群

```bash
# 终端1：节点1
./bin/ChatServer 127.0.0.1 6000

# 终端2：节点2
./bin/ChatServer 127.0.0.1 6002

# 启动 Nginx
sudo /usr/local/nginx/sbin/nginx

# 客户端连接（统一走 Nginx 网关）
./bin/ChatClient 127.0.0.1 8000
```

---

## 项目思考与收获

1. **为什么选 Muduo 而不是手写 epoll？** Muduo 封装了 Reactor 事件循环和线程池，让我可以聚焦业务逻辑，同时深入理解其内部的 Channel / EventLoop / TcpServer 分层设计。

2. **Redis Pub/Sub 的局限性**：消息不持久化，若订阅方节点崩溃会丢消息。生产级方案可引入 Redis Stream 或 Kafka 保证 at-least-once 投递。

3. **连接断开的一致性**：客户端异常断开时需原子性地从 `_userConnMap` 中移除并将用户状态置为 offline，否则其他用户仍会向其投递消息导致异常。

---

## License

MIT
