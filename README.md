# 🚀 C++ 集群聊天服务器 (Cluster Chat Server)

## 📖 项目简介
本项目是一个基于 C++11 和 Muduo 网络库实现的高并发集群聊天服务器。
项目从零剖析并剥离了传统单体网络程序的强耦合，通过引入 **Nginx 的 TCP 负载均衡**，将单机并发能力平滑扩展至分布式集群；并利用 **Redis 发布-订阅 (Pub/Sub) 模型**，作为跨服务器的消息总线，完美解决了分布式部署下的跨服通信难题。

## 🛠️ 核心技术栈
* **底层网络引擎**：基于 `epoll` 的 I/O 多路复用与多线程 Reactor 模型 (Muduo 库)
* **协议与数据序列化**：JSON 文本序列化
* **数据存储层**：MySQL 关系型数据库 (持久化用户数据、好友关系、离线消息)
* **中间件架构**：
  * **Nginx**：配置 `stream` 模块实现纯 TCP 长连接的客户端负载均衡。
  * **Redis**：独立解耦的消息队列，通过 `hiredis` 实现跨节点的单聊/群聊消息精准路由。
* **构建系统**：CMake 跨平台构建 + 自动化 Shell 脚本

## 📂 核心目录结构
```text
ChatServer/
├── bin/                    # 编译生成的可执行文件 (ChatServer, ChatClient)
├── build/                  # CMake 外部构建目录 (被 gitignore 拦截)
├── include/                # 项目核心头文件
├── src/                    # 项目源文件
│   ├── server/             # 服务端核心逻辑
│   └── client/             # 客户端核心逻辑
├── autobuild.sh            # 一键自动化编译脚本
├── CMakeLists.txt          # 顶层构建脚本
└── README.md               # 项目说明文档
```

## ⚙️ 环境依赖 (Prerequisites)
在编译运行本项目之前，请确保您的 Linux (Ubuntu) 环境已安装以下基础组件：
* `CMake` ( >= 3.10 )
* `GCC/G++` (支持 C++11 标准)
* `Muduo` 高并发网络库
* `MySQL` 服务端及开发库 (`libmysqlclient-dev`)
* `Redis` 服务端及 C 客户端库 (`hiredis`)
* `Nginx` (需源码编译并激活 `--with-stream` 模块)

## 🚀 极速构建 (Build)
本项目提供了一键自动化编译脚本，彻底解放双手，实现极速构建。

```bash
# 1. 克隆项目到本地
git clone git@github.com:TaiPingYang007/ChatServer.git
cd ChatServer

# 2. 赋予脚本执行权限并一键编译
chmod +x autobuild.sh
./autobuild.sh
```
*编译成功后，可执行文件 `ChatServer` 和 `ChatClient` 将自动生成在项目根目录的 `bin` 文件夹下。*

## 🏃 运行指南 (Run)

**1. 唤醒基础设施**
* 确保本地 MySQL 服务运行，并导入项目所需的数据库表结构。
* 确保 Redis 服务已在后台启动。
* 启动 Nginx 代理网关（监听统一网关端口，如 8000）：
```bash
sudo /usr/local/nginx/sbin/nginx
```

**2. 启动集群节点**
打开多个终端，启动多个 ChatServer 实例，挂载在不同的端口上：
```bash
cd bin
./ChatServer 127.0.0.1 6000
./ChatServer 127.0.0.1 6002
```

**3. 启动客户端集群接入**
用户无需关心后端有多少台服务器，直接向 Nginx 网关发起 TCP 连接即可：
```bash
cd bin
./ChatClient 127.0.0.1 8000
```