# C++ 集群聊天服务器

> 基于 Muduo + Redis + Nginx 的模式 A（本地开发 + Docker 跑依赖）聊天后端

## 当前模式

本项目当前采用 **模式 A**：

- 本地写代码
- 本地编译 `ChatServer` / `ChatClient`
- 本地运行 `ChatServer` / `ChatClient`
- Docker 只负责依赖服务

当前依赖服务：

- 共享 `mysql_db`
- 项目内 `redis`
- 项目内 `nginx`

## 目录说明

```text
ChatServer/
├── autobuild.sh                    # 本地构建脚本
├── compose.yaml                    # 只编排依赖服务
├── .env.example                    # 环境变量模板
├── cmake/ProjectDependencies.cmake # 本地依赖接入
├── docker/
│   ├── mysql/init/01-init-chatserver.sql
│   ├── nginx/nginx.conf
│   └── redis/redis.conf
├── scripts/bootstrap-shared-mysql.sh
├── include/
└── src/
```

## 本地开发前提

本机建议准备：

- `g++`
- `cmake`
- `default-libmysqlclient-dev`
- `libhiredis-dev`
- `libboost-dev`
- `libboost-test-dev`
- `muduo`

推荐 `muduo` 本地安装到：

```text
/usr/local
```

如果不是这个位置，请显式设置：

```bash
export MUDUO_ROOT=/你的/muduo/安装目录
```

## MySQL 约定

本项目继续复用共享 `mysql_db`，采用：

- 独立数据库 `chatserver`
- 独立用户 `chatserver_app`
- 独立权限

第一次运行前，先执行：

```bash
cp .env.example .env
MYSQL_ROOT_PASSWORD=你的_mysql_root_密码 ./scripts/bootstrap-shared-mysql.sh
```

## Docker 依赖服务启动

进入项目目录：

```bash
cd ~/projects/cpp_project/02_cluster_chat_server/ChatServer
```

启动依赖服务：

```bash
docker compose up -d
```

查看状态：

```bash
docker compose ps
```

当前 compose 只负责：

- `redis`
- `nginx`

## 本地构建

执行：

```bash
./autobuild.sh
```

或者手动执行：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

产物：

- `bin/ChatServer`
- `bin/ChatClient`

## 本地运行服务端

启动节点 1：

```bash
./bin/ChatServer 127.0.0.1 6000
```

启动节点 2：

```bash
./bin/ChatServer 127.0.0.1 6002
```

说明：

- `redis` 走 Docker 内 `6379`
- `nginx` 在 Docker 内监听 `8000`
- `nginx` 会转发到宿主机本地的：
  - `6000`
  - `6002`

## 本地运行客户端

默认走集群入口：

```bash
./bin/ChatClient 127.0.0.1 8000
```

如果你想直连某个节点：

节点 1：

```bash
./bin/ChatClient 127.0.0.1 6000
```

节点 2：

```bash
./bin/ChatClient 127.0.0.1 6002
```

## 推荐运行顺序

### 第一次

```bash
cd ~/projects/cpp_project/02_cluster_chat_server/ChatServer
cp .env.example .env
MYSQL_ROOT_PASSWORD=你的_mysql_root_密码 ./scripts/bootstrap-shared-mysql.sh
docker compose up -d
./autobuild.sh
./bin/ChatServer 127.0.0.1 6000
./bin/ChatServer 127.0.0.1 6002
./bin/ChatClient 127.0.0.1 8000
```

### 日常开发

```bash
cd ~/projects/cpp_project/02_cluster_chat_server/ChatServer
docker compose up -d
./autobuild.sh
./bin/ChatServer 127.0.0.1 6000
./bin/ChatServer 127.0.0.1 6002
./bin/ChatClient 127.0.0.1 8000
```

## 日志与排查

看依赖服务日志：

```bash
docker compose logs -f redis
docker compose logs -f nginx
```

如果客户端提示：

```text
this account is using
```

说明数据库里用户状态残留为 `online`，可以清理：

```bash
docker exec -it mysql_db mysql -uchatserver_app -pchatserver_dev_password -D chatserver -e "UPDATE User SET state='offline' WHERE state='online'; SELECT id,name,state FROM User;"
```

## 集群验证方式

1. 启动 `redis` 和 `nginx`
2. 本地开两个 `ChatServer` 节点：
   - `6000`
   - `6002`
3. 本地开两个客户端
4. 一个客户端连 `8000`
5. 再结合服务端日志，确认不同节点之间能通过 Redis 转发消息

## 当前设计说明

现在的主路线是：

- 本地开发 / 本地构建 / 本地运行
- Docker 只作为依赖环境

这比 Docker-first 更贴近常见 C++ 后端团队开发方式。
