# ChatServer Project Context

## 项目定位与运行模式

- 这是一个基于 Muduo、Redis、MySQL、Nginx 的 C++ 集群聊天服务器项目。
- 当前采用 Mode A：
  - 本地写代码
  - 本地编译 `ChatServer` / `ChatClient`
  - 本地运行服务端和客户端
  - Docker 只负责依赖服务，不直接承载业务进程
- 项目目标不是单节点聊天，而是通过 Nginx + 多个 ChatServer 节点 + Redis 发布订阅实现集群入口和跨节点投递。

## 技术栈与依赖前提

- 语言与构建：
  - C++11
  - CMake 3.16+
- 核心依赖：
  - Muduo
  - MySQL client (`mysqlclient`)
  - hiredis
  - nlohmann_json
  - pthread / `Threads::Threads`
- 运行辅助：
  - Docker Compose
  - Nginx
  - Redis
- 本地依赖默认假设：
  - Muduo 安装在 `/usr/local`
  - 如不在默认路径，需要显式设置 `MUDUO_ROOT`

## 系统拓扑

```text
ChatClient
  -> Nginx :8000
  -> ChatServer node A :6000
  -> ChatServer node B :6002

ChatServer
  -> Redis pub/sub
  -> MySQL chatserver
```

- 客户端默认连接 `127.0.0.1:8000`，由 Nginx `stream` 转发到本机两个服务端节点。
- 集群内消息分发路径是：
  - 优先投递给当前节点上的在线连接
  - 当前节点没有目标连接时，查询用户状态
  - 目标用户在线但在其他节点时，通过 Redis 发布订阅跨节点转发
  - 目标用户离线时，写入离线消息表

## 核心代码结构

### 入口与构建

- 顶层构建入口：`CMakeLists.txt`
- 子目录构建入口：`src/CMakeLists.txt`
- 服务端目标：`src/server/CMakeLists.txt`
- 客户端目标：`src/client/CMakeLists.txt`
- 本地构建脚本：`autobuild.sh`

### 服务端职责分层

- `ChatServer`
  - 位于 `src/server/chatserver.cpp`
  - 负责 Muduo `TcpServer`、连接回调、消息回调、换行分帧、JSON 解析入口
  - 默认设置 4 个 Muduo 线程
- `ChatService`
  - 位于 `src/server/chatservice.cpp`
  - 负责业务消息路由和核心业务逻辑
  - 使用单例模式维护消息处理器映射、在线连接表、Redis 模块和各类 Model
- `model/*`
  - 负责各业务对象的数据库读写
  - 主要包括用户、好友、群组、离线消息、好友申请、加群申请
- `db/*`
  - 负责 MySQL 连接与 SQL 执行
  - MySQL 连接参数从环境变量读取
- `redis/*`
  - 负责 Redis 发布订阅、监听线程、订阅命令队列和跨节点消息接收
- `logger.*`
  - 负责日志初始化与日志输出

### 客户端职责

- 客户端入口位于 `src/client/main.cpp`
- 客户端使用阻塞 socket + 独立读线程
- 主线程负责命令行菜单和消息发送，读线程负责接收服务器响应和推送消息

## 协议与消息边界

- 传输格式是 JSON 文本。
- 报文分隔方式是“每条 JSON 后追加一个换行符 `\n`”。
- 服务端 `onMessage` 会按连接缓冲区累计数据，并按换行符切分完整报文。
- 消息类型定义位于 `include/public.hpp`，核心消息族包括：
  - 登录 / 登录响应 / 登出
  - 注册 / 注册响应
  - 私聊 / 私聊响应
  - 添加好友请求 / 处理好友请求 / 好友请求响应
  - 创建群组 / 创建群组响应
  - 加群请求 / 处理加群请求 / 加群响应
  - 群聊
- `ChatService` 通过 `msgid -> handler` 的映射分发业务逻辑。

## 数据模型

### 主要业务对象

- `User`
- `Friend`
- `Group` / `GroupUser`
- `OfflineMessage`
- `FriendRequest`
- `GroupRequest`

### 对应数据库表

- `User`
- `Friend`
- `AllGroup`
- `GroupUser`
- `OfflineMessage`
- `FriendRequest`
- `GroupRequest`

### 当前数据职责概览

- `User`
  - 注册、登录、在线状态维护
- `Friend`
  - 好友关系校验与好友列表查询
- `AllGroup` / `GroupUser`
  - 群信息、群成员、群角色
- `OfflineMessage`
  - 离线消息暂存
- `FriendRequest` / `GroupRequest`
  - 申请状态：`pending` / `accepted` / `rejected`

## 构建与启动基线

### 环境变量

- 服务端依赖 `.env` 中的 MySQL / Redis 连接参数
- 默认模板文件：`.env.example`
- 关键变量：
  - `MYSQL_HOST`
  - `MYSQL_PORT`
  - `MYSQL_DATABASE`
  - `MYSQL_USER`
  - `MYSQL_PASSWORD`
  - `REDIS_HOST`
  - `REDIS_PORT`
  - `REDIS_PASSWORD`

### MySQL 准备

- 项目复用共享 `mysql_db`
- 初始化脚本：`scripts/bootstrap-shared-mysql.sh`
- 默认数据库名：`chatserver`
- 默认应用用户：`chatserver_app`

### Docker 依赖服务

- 编排文件：`compose.yaml`
- 当前 compose 只负责：
  - `redis`
  - `nginx`

### 本地构建

- 推荐脚本：`./autobuild.sh`
- 或手动：
  - `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`
  - `cmake --build build -j"$(nproc)"`

### 本地运行

- 启动依赖服务：
  - `docker compose up -d`
- 启动前加载环境变量：
  - `set -a`
  - `source .env`
  - `set +a`
- 启动两个服务端节点：
  - `./bin/ChatServer 0.0.0.0 6000`
  - `./bin/ChatServer 0.0.0.0 6002`
- 启动客户端：
  - `./bin/ChatClient 127.0.0.1 8000`

## 学习与复习入口

推荐阅读顺序：

1. `README.md`
2. `include/public.hpp`
3. `src/server/main.cpp`
4. `src/server/chatserver.cpp`
5. `include/server/chatservice.hpp`
6. `src/server/chatservice.cpp`
7. `src/server/db/db.cpp`
8. `src/server/redis/redis.cpp`
9. `docker/mysql/init/01-init-chatserver.sql`
10. `src/client/main.cpp`

带着问题复习时，优先按下面的映射找代码：

- 想看网络收包和分帧：`src/server/chatserver.cpp`
- 想看消息分发和业务主流程：`src/server/chatservice.cpp`
- 想看跨节点投递：`src/server/chatservice.cpp` + `src/server/redis/redis.cpp`
- 想看数据库结构：`docker/mysql/init/01-init-chatserver.sql`
- 想看客户端交互：`src/client/main.cpp`
- 想看部署入口：`compose.yaml` + `docker/nginx/nginx.conf`

## 常见约定与排障提示

- `ChatServer` 在本地模式下必须绑定 `0.0.0.0`，不能只绑定 `127.0.0.1`。
- 原因是 Nginx 运行在 Docker 容器内，需要通过宿主机地址转发到本地服务端节点。
- 服务端和依赖连接参数默认依赖环境变量，不加载 `.env` 时容易连错默认值或误判问题来源。
- 如果客户端收到 `this account is using`，通常不是当前连接逻辑有问题，而是数据库中的该用户状态残留为 `online`。
- 服务端异常退出时，`ChatService::reset()` 会尝试把在线状态重置为离线，但调试中仍要警惕脏状态残留。
- 当前日志目录由构建时的 `CHATSERVER_LOG_DIR` 指向 `bin/`。

## Graphify 状态

- 本机已安装 `graphify`，适合作为后续项目图谱和结构化复习工具。
- 本次初始化已尝试执行：
  - `graphify extract . --out .`
- 当前未生成图谱产物，原因是环境中没有可用的 LLM API key。
- 后续如果配置了 `OPENAI_API_KEY`、`GEMINI_API_KEY`、`GOOGLE_API_KEY`、`ANTHROPIC_API_KEY` 或 `MOONSHOT_API_KEY`，可以重新补跑：
  - `graphify extract . --out .`
  - `graphify tree --graph graphify-out/graph.json`
  - `graphify export callflow-html --graph graphify-out/graph.json`
