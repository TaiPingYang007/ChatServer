# C++ 集群聊天服务器

> 基于 Muduo + Redis + Nginx 的 Docker-first 集群聊天后端

## 项目简介

这是一个支持多节点部署的即时通讯后端。当前仓库已经整理成 **Docker-only** 运行方式：

- `chatserver-1`：节点 1，监听 `6000`
- `chatserver-2`：节点 2，监听 `6002`
- `redis`：节点间消息总线
- `nginx`：统一 TCP 入口，监听 `8000`
- `client`：按需启动的交互式客户端容器

整体数据流：

```text
client -> nginx:8000 -> chatserver-1 / chatserver-2
                        |             |
                        +---- Redis ---+
                              |
                            MySQL
```

## 当前运行方式

本项目当前是 **Docker-first / Docker-only**：

- 不再维护 `./autobuild.sh` 本机构建入口
- 构建、依赖安装、运行全部放在 Docker 内完成
- 客户端也通过 Docker 启动

## 目录说明

```text
ChatServer/
├── compose.yaml                    # 整套服务编排
├── Dockerfile                      # 服务端 / 客户端镜像构建
├── .env.example                    # 环境变量模板
├── cmake/ProjectDependencies.cmake # CMake 依赖接入逻辑
├── docker/
│   ├── mysql/init/01-init-chatserver.sql
│   ├── nginx/nginx.conf
│   └── redis/redis.conf
├── scripts/bootstrap-shared-mysql.sh
├── include/
└── src/
```

## 依赖前提

你需要提前准备：

- Docker Engine
- Docker Compose v2
- 一个已经在宿主机运行的共享 MySQL 容器 `mysql_db`

这个仓库 **不会** 在 `compose.yaml` 里再起一个新的 MySQL。

## MySQL 约定

当前项目采用：

- 共享一个 MySQL 实例
- 每个项目独立数据库
- 每个项目独立用户
- 每个项目独立权限

本项目默认约定：

- 数据库：`chatserver`
- 用户：`chatserver_app`
- 密码：`chatserver_dev_password`

## Redis 约定

Redis 由本项目自己的 compose 栈管理：

- 容器内服务名：`redis`
- 端口：`6379`
- 默认密码：`redis_dev_password`

## 环境变量

先复制模板：

```bash
cp .env.example .env
```

默认内容如下：

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

说明：

- `MYSQL_HOST=host.docker.internal` 表示服务端容器通过宿主机端口连接共享 `mysql_db`
- `REDIS_HOST=redis` 表示服务端容器通过 compose 网络内服务名连接 Redis

## 第一次运行

### 1. 初始化共享 MySQL

执行：

```bash
MYSQL_ROOT_PASSWORD=你的_mysql_root_密码 ./scripts/bootstrap-shared-mysql.sh
```

这个脚本会：

- 创建数据库 `chatserver`
- 创建用户 `chatserver_app`
- 授权这个用户只访问 `chatserver`
- 执行表结构初始化 SQL

对应文件：

- [scripts/bootstrap-shared-mysql.sh](scripts/bootstrap-shared-mysql.sh)
- [docker/mysql/init/01-init-chatserver.sql](docker/mysql/init/01-init-chatserver.sql)

### 2. 如果你的网络环境要求 Docker 走代理

如果你已经在 WSL 全局安装了这些命令，可以按需使用：

```bash
docker-proxy-on
docker-proxy-status
```

如果当前网络直连 Docker Hub 就够用，这一步可以跳过。

### 3. 构建镜像

```bash
docker compose build
```

### 4. 启动服务端栈

```bash
docker compose up -d
```

### 5. 检查服务状态

```bash
docker compose ps
```

你应该看到：

- `redis`：`healthy`
- `chatserver-1`：`Up`
- `chatserver-2`：`Up`
- `nginx`：`Up`

## 日常启动方式

以后最常用的一套命令就是：

```bash
cd ~/projects/cpp_project/02_cluster_chat_server/ChatServer
docker-proxy-on
docker compose build
docker compose up -d
docker compose ps
docker compose run --rm client
```

如果你只是重新启动服务，不改代码，也可以不重新 build：

```bash
docker compose up -d
```

如果你改了代码，建议重新 build 并强制重建容器：

```bash
docker compose build
docker compose up -d --force-recreate
```

## 客户端运行方式

### 标准方式：走集群入口

```bash
docker compose run --rm client
```

这个客户端默认连接：

```text
nginx:8000
```

### 直连某个节点

直连节点 1：

```bash
docker compose run --rm --entrypoint ./bin/ChatClient client chatserver-1 6000
```

直连节点 2：

```bash
docker compose run --rm --entrypoint ./bin/ChatClient client chatserver-2 6002
```

## 端口说明

- `8000`：Nginx 对外统一入口
- `6000`：`chatserver-1`
- `6002`：`chatserver-2`

客户端默认应该优先走 `8000`。

## 常用命令

### 查看状态

```bash
docker compose ps
docker ps
docker images
```

### 查看日志

```bash
docker compose logs
docker compose logs -f
docker compose logs -f chatserver-1
docker compose logs -f chatserver-2
docker compose logs -f redis
docker compose logs -f nginx
```

### 停止服务

只停止：

```bash
docker compose stop
```

停止并删除当前项目容器和网络：

```bash
docker compose down
```

## 如何确认“集群真的在运行”

### 配置层

当前 `nginx` upstream 已经配置为：

- `chatserver-1:6000`
- `chatserver-2:6002`

文件：

- [docker/nginx/nginx.conf](docker/nginx/nginx.conf)

### 运行层

执行：

```bash
docker compose ps
```

如果四个服务都正常，说明集群栈已经起来了。

### 业务层

真正确认“集群生效”，建议：

1. 开两个客户端
2. 注册 / 登录两个不同用户
3. 配合：

```bash
docker compose logs -f chatserver-1 chatserver-2
```

观察两个用户是否落到不同节点，以及消息是否能跨节点转发。

## 已知现象与处理方法

### 1. 登录提示 `this account is using`

这通常不是客户端容器没删干净，而是：

- MySQL 中用户的 `state` 还停留在 `online`

可以手动清理：

```bash
docker exec -it mysql_db mysql -uchatserver_app -pchatserver_dev_password -D chatserver -e "UPDATE User SET state='offline' WHERE state='online'; SELECT id,name,state FROM User;"
```

### 2. 客户端想正常退出

登录前主菜单直接输入：

```text
3
```

登录后更推荐先执行：

```text
loginout
```

### 3. 客户端卡住或无法退出

在另一个终端里清理临时 client 容器：

```bash
ids=$(docker ps -aq --filter "name=chatserver-client-run")
[ -n "$ids" ] && docker rm -f $ids
```

### 4. `docker compose` 提示 `no configuration file provided`

说明你不在 `compose.yaml` 所在目录。  
先进入：

```bash
cd ~/projects/cpp_project/02_cluster_chat_server/ChatServer
```

再执行 `docker compose ...`

## 构建说明

当前镜像构建时会：

1. 安装系统依赖
2. 拉取并编译 `muduo`
3. 拉取 `nlohmann-json`
4. 编译：
   - `ChatServer`
   - `ChatClient`

所以第一次 build 较慢是正常的。

## 总结

现在这个项目的标准运行方式可以压缩成：

```bash
cd ~/projects/cpp_project/02_cluster_chat_server/ChatServer
docker-proxy-on
docker compose build
docker compose up -d
docker compose run --rm client
```

这就是以后你最常用的启动路径。
