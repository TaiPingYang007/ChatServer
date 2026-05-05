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
| 构建 | CMake + Docker | 由 Docker 镜像构建流程统一编译 |

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
├── cmake/                          # CMake 依赖接入逻辑
├── compose.yaml                    # Docker 开发栈（2 个 ChatServer + Redis + Nginx + client）
├── docker/                         # Redis / Nginx / MySQL schema 配置
├── scripts/                        # 数据库 bootstrap 脚本
└── CMakeLists.txt
```

---

## 环境依赖

| 依赖 | 版本要求 | 安装参考 |
|------|---------|---------|
| Docker Engine | 最新稳定版 | 用于运行 ChatServer / Redis / Nginx 容器 |
| Docker Compose | v2 | `docker compose ...` |

---

## 快速开始

本项目当前是 **Docker-only**：官方支持的启动方式只有 Docker Compose，不再维护原生开发入口。

本项目当前默认复用外部共享 MySQL 容器 `mysql_db`，**不会**在本仓库的 `compose.yaml` 中再起一个新的 MySQL。

1. 准备应用配置

```bash
cp .env.example .env
```

`.env` 中的默认值已经和 Docker 开发栈对齐：

```env
MYSQL_HOST=host.docker.internal
MYSQL_PORT=3306
MYSQL_DATABASE=chatserver
MYSQL_USER=chatserver_app
MYSQL_PASSWORD=chatserver_dev_password
REDIS_HOST=redis
REDIS_PORT=6379
REDIS_PASSWORD=redis_dev_password
```

2. 初始化共享 MySQL 中的库、用户、权限和表

```bash
chmod +x scripts/bootstrap-shared-mysql.sh
MYSQL_ROOT_PASSWORD=你的_mysql_root_密码 ./scripts/bootstrap-shared-mysql.sh
```

脚本会连接正在运行的 `mysql_db` 容器，并完成：

- 创建数据库 `chatserver`
- 创建应用用户 `chatserver_app`
- 授权 `chatserver_app` 仅访问 `chatserver`
- 执行 [`docker/mysql/init/01-init-chatserver.sql`](docker/mysql/init/01-init-chatserver.sql) 中的表结构

3. 启动完整集群开发栈

```bash
docker compose up --build -d
```

默认会长期启动 4 个服务：

- `chatserver-1`：监听 `6000`
- `chatserver-2`：监听 `6002`
- `redis`：跨节点 Pub/Sub
- `nginx`：统一 TCP 网关，监听 `8000`

4. 按需启动客户端容器

```bash
docker compose run --rm client
```

这个 `client` 服务通过 compose profile 标记为按需工具，所以不会随着 `docker compose up` 一起常驻启动；当你显式执行 `docker compose run --rm client` 时，它会加入同一个 compose 网络并直接连接 `nginx:8000`。

如果将来你需要本机调试方式，可以自己再扩展，但当前仓库官方只支持 Docker 路线。

---

## 项目思考与收获

1. **为什么选 Muduo 而不是手写 epoll？** Muduo 封装了 Reactor 事件循环和线程池，让我可以聚焦业务逻辑，同时深入理解其内部的 Channel / EventLoop / TcpServer 分层设计。

2. **Redis Pub/Sub 的局限性**：消息不持久化，若订阅方节点崩溃会丢消息。生产级方案可引入 Redis Stream 或 Kafka 保证 at-least-once 投递。

3. **连接断开的一致性**：客户端异常断开时需原子性地从 `_userConnMap` 中移除并将用户状态置为 offline，否则其他用户仍会向其投递消息导致异常。

---

## License

MIT
