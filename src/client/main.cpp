/*
我现在自己写了一个客户端应用程序。代码里的 127.0.0.1:6000
是我要去连接的目标服务器的地址和端口（就跟之前用 telnet
指定的一样）。而我的客户端程序自己身上用于发报和接收的真正通信端口（接口），并不是我自己写的，而是代码跑起来、用户真正触碰到底层
connect() 函数发起连接的那一瞬间，由操作系统临时动态分配给这个程序的！
*/

#include "../../include/public.hpp"
#include "../../include/server/model/group.hpp"
#include "../../include/server/model/user.hpp"
#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <ctime>
#include <functional>
#include <iostream>
#include <netinet/in.h>
#include <nlohmann/json.hpp>
#include <semaphore.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>

// 记录当前系统登陆的用户信息
User g_currentUser;

// 记录当前登录用户的好友列表
std::vector<User> g_currentUserFriendList;

// 记录当前用户的群组列表信息
std::vector<Group> g_currentUserGroupList;

// 控制主菜单页面程序
bool isMainMenuRuning = false;

// 用于读写线程之间的通信
sem_t rwsem;

// 记录登录状态
std::atomic_bool g_isLoginSuccess{false};

// 接受线程
void readTaskHandler(int clinetfd);

// 获取系统时间（聊天信息需要添加时间信息）
std::string getCurrentTime();

// 显示当前登录成功用户的基本信息
void showCurrentUesrInfo();

// 主聊天页面程序
void mainMenu(int clientfd);

// 聊天客户端程序实现，main线程用作发送线程，子线程用作接受线程
int main(int argc, char **argv) {
  // 检查用户输入
  if (argc < 3) {
    std::cerr << "command invalid! example: ./ChatClient 127.0.0.1 6000"
              << "\n";
    exit(-1);
  }

  // 初始化读写线程通信用的信号量
  sem_init(&rwsem, 0, 0);

  // 解析通过命令行参数传递ip和port
  char *ip = argv[1];
  uint16_t port = atoi(argv[2]); // uint16_t是无符号16位整数

  // 创建clinet端的socket
  int clientfd = socket(AF_INET, SOCK_STREAM, 0);
  if (clientfd == -1) {
    std::cerr << "socket create error\n";
    exit(-1);
    // exit(0)：代表程序正常、顺利地运行结束（Success）。就像你在外面吃完饭，付了钱，推开门正常离开。
    // exit(-1)（或者任何非 0 的数字比如
    // exit(1)）：代表程序遇到了无法挽回的致命错误，被迫异常终止
    // 可以使用  echo $?  命令查看程序退出状态
  }

  // 填写clinet需要连接的server信息，ip+port
  sockaddr_in server{}; // 列表初始化，将所有成员初始化为0 {}的效果同下
  // std::memset(&server, 0, sizeof(sockaddr_in)); //
  // 初始化server结构体的内存空间

  server.sin_family = AF_INET;
  server.sin_port = htons(port);
  server.sin_addr.s_addr = inet_addr(
      ip); // inet_addr 把你认识的 IP 字符串翻译成网卡认识的二进制数字。

  // clinet和server进行连接 将文件描述符与（协议加端口加ip）绑定
  if (connect(clientfd, (sockaddr *)&server, sizeof(sockaddr_in)) == -1) {
    std::cerr << "connect server fail!" << "\n";
    close(clientfd);
    exit(-1);
  }

  // 连接服务器成功启动接收子线程
  std::thread readTask(readTaskHandler,
                       clientfd); // 创建线程，参数是函数体和函数体需要的参数
  // 你告诉系统：“这个子线程我已经把它放生了，它以后在后台生老病死全靠自己，不要把它和我这个
  // readTask 局部对象的死活相绑定了。”
  readTask.detach();

  // main 线程用于接受用户输入，负责发送数据
  while (true) {
    // 显示首页菜单 登录 注册 退出
    std::cout << "========================\n";
    std::cout << "1. login\n";
    std::cout << "2. register\n";
    std::cout << "3. quit\n";
    std::cout << "========================\n";
    std::cout << "choice: ";
    std::string choice_Str;
    std::cin >> choice_Str;
    std::cin.get(); // 消费掉输入choice后留下的换行符

    // 检查用户输入
    if (choice_Str.empty() || choice_Str.length() > 1) {
      std::cerr << "invalid input!\n";
      continue;
    }

    char choice = choice_Str[0];
    switch (choice) {
    case '1': // login业务
    {
      // 用户使用用户名和密码登录
      char name[50] = {0};
      char pwd[50] = {0};
      std::cout << "username：";
      std::cin.getline(name, 50);
      std::cout << "userpassword：";
      std::cin.getline(pwd, 50);

      nlohmann::json js;
      js["msgid"] = EnMsgType::LOGIN_MSG;
      js["name"] = name;
      js["password"] = pwd;
      std::string request = js.dump() + "\n";

      // 在 C/C++ 的世界里，所有的字符串（无论你用的是普通的 char[] 还是高级的
      // std::string），在内存的最后面，都必定隐藏着一个你看不到的字符：\0
      int len = send(clientfd, request.c_str(), request.size(), 0);
      if (len == -1) {
        std::cerr << "send login msg error：" << request << "\n";
      }

      sem_wait(&rwsem); // 等待子线程登录响应

      if (g_isLoginSuccess) {
        // 进入聊天主菜单页面
        isMainMenuRuning = true;
        mainMenu(clientfd);
      }
    } break;
    case '2': // register业务
    {
      /*
      std::cin >> 是以**“空格、Tab、回车”**作为分隔符的。
      假设用户设置的密码是 "I love C++"（中间有空格）。如果你用 std::cin
      >> pwd;，程序只会把 "I" 读进密码里，然后把 "love C++"
      留在缓冲区里，直接导致后面的读取全部错乱。
      为了能够把一整行（包括空格）完整读下来，我们就必须用 getline。

      这行代码 std::cin.getline(name, 50); 向 C++ 下达了三个指令：

      读到哪？ —— 读到 name 这个我们刚才申请的 50 字节的数组里。
      最多读多少？ —— 传入 50。告诉它最多只能读 49
      个有效字符，系统会自动在最后强行补上一个
      \0（字符串结束符）。哪怕用户在键盘上发疯拍了 1000
      个字母，程序也只会安全地截取前 49 个，绝对不会发生内存溢出（Buffer
      Overflow）导致程序崩溃。 什么时候停？ —— 直到读取到用户按下的
      回车键（\n）为止。并且它会自动把这个丢人的回车符从输入缓冲区里“吃掉”并抛弃。
      */

      char name[50] = {0};
      char pwd[50] = {0};
      std::cout << "username：";
      std::cin.getline(name, 50);
      std::cout << "userpassword：";
      std::cin.getline(pwd, 50);

      nlohmann::json js;
      js["msgid"] = EnMsgType::REG_MSG;
      js["name"] = name;
      js["password"] = pwd;
      std::string request = js.dump() + "\n";

      // 而是Linux 操作系统直接提供的、非常低级的 C 语言网络发送函数（包含在
      // <sys/socket.h> 头文件里）
      // 参数是你要发给谁（文件描述符），发什么（json字符串），发多大的内容，特殊要求（没有填0）
      int len = send(clientfd, request.c_str(), request.size(), 0);
      if (len == -1) {
        std::cerr << "send reg msg error：" << request << "\n";
      }

      sem_wait(&rwsem); // 等待子线程注册响应
    } break;
    case '3': // quit业务
      close(clientfd);
      sem_destroy(&rwsem);
      exit(0);
    default:
      std::cout << "invalid input!\n";
      break;
    }
  }
}

// 处理注册响应逻辑
void doRegResponse(nlohmann::json &responsejs) {
  if (responsejs["errno"].get<int>() != 0) // 注册失败
  {
    std::cerr << "Username is already exist，register error!" << "\n";
  } else // 注册成功
  {
    std::cout << "register success,userid is " << responsejs["id"].get<int>()
              << "，do not forget it!\n";
  }
}

// 处理登陆响应逻辑
void doLoginResponse(nlohmann::json &responsejs) {
  int res_errno = responsejs["errno"].get<int>();

  switch (res_errno) {
  case 0: // 登陆成功
  {
    // 记录当前用户的id和name
    g_currentUser.setId(responsejs["id"].get<int>());
    g_currentUser.setName(responsejs["name"].get<std::string>());

    // 记录当前用户的好友列表信息
    if (responsejs.contains("friends")) {
      std::vector<std::string> vec = responsejs["friends"];
      for (std::string &str : vec) {
        nlohmann::json js = nlohmann::json::parse(str);
        User user;
        user.setId(js["id"].get<int>());
        user.setName(js["name"]);
        user.setState(js["state"]);
        g_currentUserFriendList.emplace_back(user);
      }
    }

    // 记录当前用户的群组列表信息
    if (responsejs.contains("groups")) {
      std::vector<std::string> vec = responsejs["groups"];
      for (std::string &str : vec) {
        nlohmann::json grpjs = nlohmann::json::parse(str);
        Group group;
        group.setId(grpjs["id"]);
        group.setName(grpjs["name"]);
        group.setDesc(grpjs["desc"]);

        std::vector<std::string> vec2 = grpjs["users"];
        for (std::string &str : vec2) {
          GroupUser user;
          nlohmann::json userjs = nlohmann::json::parse(str);
          user.setId(userjs["id"]);
          user.setName(userjs["name"]);
          user.setState(userjs["state"]);
          user.setRole(userjs["role"]);
          group.getUsers().emplace_back(user);
        }
        g_currentUserGroupList.emplace_back(group);
      }
    }

    // 显示登录用户的基本信息
    showCurrentUesrInfo();

    // 显示当前用户的离线消息 个人消息和群组消息
    if (responsejs.contains("offlinemsg")) {
      std::vector<std::string> vec = responsejs["offlinemsg"];
      for (std::string &str : vec) {
        nlohmann::json msgjs = nlohmann::json::parse(str);
        // 先判断是群组消息还是私聊消息
        if (msgjs["msgid"].get<int>() ==
            static_cast<int>(EnMsgType::ONE_CHAT_MSG)) {
          // time:[userid]username，said:msg
          std::cout << msgjs["time"].get<std::string>() << ":["
                    << msgjs["userid"].get<int>() << "]"
                    << msgjs["name"].get<std::string>()
                    << "，said:" << msgjs["msg"].get<std::string>() << "\n";
        } else if (msgjs["msgid"].get<int>() ==
                   static_cast<int>(EnMsgType::GROUP_CHAT_MSG)) {
          // time:[groupid]groupname:[userid]username，said:msg
          std::cout << msgjs["time"].get<std::string>() << ":["
                    << msgjs["groupid"] << "]"
                    << msgjs["groupname"].get<std::string>() << ":["
                    << msgjs["userid"].get<int>() << "]"
                    << msgjs["name"].get<std::string>()
                    << "，said:" << msgjs["msg"].get<std::string>() << "\n";
        }
      }
    }
    g_isLoginSuccess = true;
  } break;
  case 1: // 登录用户已登陆
    g_isLoginSuccess = false;
    std::cerr << responsejs["errmsg"] << "\n";
    break;
  case 2: // 密码错误
    g_isLoginSuccess = false;
    std::cerr << responsejs["errmsg"] << "\n";
    break;
  case 3: // 用户不存在
    g_isLoginSuccess = false;
    std::cerr << responsejs["errmsg"] << "\n";
    break;
  default: // 错误
    g_isLoginSuccess = false;
    std::cerr << "unknow login error!" << "\n";
    break;
  }
}

// 接受线程
void readTaskHandler(int clientfd) {
  // recvbuf 需要定义到while循环外，因为要处理半包情况
  std::string recvbuf;
  while (true) {
    char buffer[1024] = {0};
    int len = recv(clientfd, buffer, 1024, 0); // 阻塞
    if (len == -1 || len == 0) {
      close(clientfd);
      exit(-1);
    }

    // 接受ChatServer转发的数据
    recvbuf.append(buffer, len);
    // find的返回值是size_t无符号整数不能用-1来判断是否找到，应该用std::string::npos来判断
    size_t pos = 0;
    // 循环找到recvbuf里所有的完整消息（以'\n'结尾）并处理，可能会有多条消息
    while ((pos = recvbuf.find('\n')) !=
           std::string::npos) // std::string::npos 就是“没找到”的代号。
    {
      std::string onMessage = recvbuf.substr(0, pos); // 提取一条完整的消息
      recvbuf.erase(0, pos + 1); // 从那里开始，删掉多少个字节

      // 保持代码健壮性，如果提取到的消息是空字符串，就继续循环等待下一条消息
      if (onMessage.empty()) {
        continue;
      }

      nlohmann::json js;
      try {
        js = nlohmann::json::parse(onMessage);
      } catch (const std::exception &e) {
        std::cerr << "parse msg error: " << e.what() << ", raw: " << onMessage
                  << "\n";
        continue;
      }

      // 处理私聊消息
      if (static_cast<int>(EnMsgType::ONE_CHAT_MSG) == js["msgid"].get<int>()) {
        std::cout << js["time"].get<std::string>() << ":["
                  << js["userid"].get<int>() << "]"
                  << js["name"].get<std::string>()
                  << "，said:" << js["msg"].get<std::string>() << "\n";
      }

      // 处理私聊响应消息
      if (static_cast<int>(EnMsgType::ONE_CHAT_MSG_ACK) ==
          js["msgid"].get<int>()) {
        std::cout << js["errmsg"].get<std::string>() << "\n";
      }

      // 处理好友请求
      if (js["msgid"].get<int>() ==
          static_cast<int>(EnMsgType::ADD_FRIEND_REQUEST)) {
        int requestUserId = js["userid"].get<int>();
        std::cout << "\n============================================\n"
                  << "[系统通知] 叮咚！用户 " << requestUserId
                  << " 申请加您为好友！\n"
                  << "同意请输入: acceptfriend:" << requestUserId << "\n"
                  << "拒绝请输入: rejectfriend:" << requestUserId << "\n"
                  << "============================================\n";
      }

      // 处理添加好友响应
      if (js["msgid"].get<int>() ==
          static_cast<int>(EnMsgType::ADD_FRIEND_RESPONSE)) {
        std::cout << "\n[系统通知] " << js["errmsg"].get<std::string>() << "\n";
      }

      // 处理创建群响应
      if (js["msgid"].get<int>() ==
          static_cast<int>(EnMsgType::CREATE_GROUP_MSG_ACK)) {
        std::cout << "\n[系统通知] " << js["errmsg"].get<std::string>() << "\n";
      }

      // 处理加群请求
      if (js["msgid"].get<int>() ==
          static_cast<int>(EnMsgType::ADD_GROUP_MSG)) {
        int requestUserId = js["userid"].get<int>();
        std::cout << "\n============================================\n"
                  << "[系统通知] 叮咚！用户 " << requestUserId
                  << " 申请加入你的群聊：" << js["groupid"].get<int>() << "！\n"
                  << "同意请输入: acceptaddgroup:" << requestUserId << ":"
                  << js["groupid"].get<int>() << "\n"
                  << "拒绝请输入: rejectaddgroup:" << requestUserId << ":"
                  << js["groupid"].get<int>() << "\n"
                  << "============================================\n";
      }

      // 处理加群响应
      if (js["msgid"].get<int>() ==
          static_cast<int>(EnMsgType::ADD_GROUP_RESPONSE)) {
        std::cout << "\n[系统通知]" << js["errmsg"].get<std::string>() << "\n";
      }

      // 处理群聊消息
      if (js["msgid"].get<int>() ==
          static_cast<int>(EnMsgType::GROUP_CHAT_MSG)) {
        std::cout << js["time"].get<std::string>() << ":["
                  << js["groupid"].get<int>() << "]"
                  << js["groupname"].get<std::string>() << ":["
                  << js["userid"].get<int>() << "]"
                  << js["name"].get<std::string>()
                  << "，said:" << js["msg"].get<std::string>() << "\n";
      }

      // 处理登陆响应消息
      if (js["msgid"].get<int>() == static_cast<int>(EnMsgType::LOG_MSG_ACK)) {
        doLoginResponse(js); // 处理登陆相应的业务逻辑
        sem_post(&rwsem);    // 唤醒主线程
        continue;
      }

      // 处理注册响应
      if (js["msgid"].get<int>() == static_cast<int>(EnMsgType::REG_MSG_ACK)) {
        doRegResponse(js); // 处理注册相应的业务逻辑
        sem_post(&rwsem);  // 唤醒主线程
        continue;
      }
    }
  }
}

// "help" command handler
void help(int fd = 0, std::string str = "");
// "chat" command handler
void chat(int, std::string);
// "addfriend" command handler
void addfriend(int, std::string);
// "acceptfriend" command handler
void acceptaddfriend(int, std::string);
// "rejectfriend" command handler
void rejectaddfriend(int, std::string);
// "creategroup" command handler
void creategroup(int, std::string);
// "addgroup" command handler
void addgroup(int, std::string);
// "acceptaddgroup" command handler
void acceptaddgroup(int, std::string);
// "rejectaddgroup" command handler
void rejectaddgroup(int, std::string);
// "groupchat" command handler
void groupchat(int, std::string);
// "loginout" command handler
void loginout(int, std::string);

// 系统支持的客户端命令列表
std::unordered_map<std::string, std::string> commandMap = {
    {"help", "显示所有支持的命令,格式：help"},
    {"chat", "一对一聊天,格式：chat:friendid:message"},
    {"addfriend", "添加好友,格式：addfriend:friendid"},
    {"acceptfriend", "同意添加好友,格式：acceptfriend:friendid"},
    {"rejectfriend", "拒绝添加好友,格式：rejectfriend:friendid"},
    {"creategroup", "创建群组,格式：creategroup:groupname:groupdesc"},
    {"addgroup", "加入群组,格式：addgroup:groupid"},
    {"acceptaddgroup", "同意加群,格式：acceptaddgroup:userid:groupid"},
    {"rejectaddgroup", "拒绝加群,格式：rejectaddgroup:userid:groupid"},
    {"groupchat", "群聊,格式：groupchat:groupid:message"},
    {"loginout", "注销,格式：loginout"}};

// 注册客户端支持的命令处理
std::unordered_map<std::string, std::function<void(int, std::string)>>
    commandHandlerMap = {{"help", help},
                         {"chat", chat},
                         {"addfriend", addfriend},
                         {"acceptfriend", acceptaddfriend},
                         {"rejectfriend", rejectaddfriend},
                         {"creategroup", creategroup},
                         {"addgroup", addgroup},
                         {"acceptaddgroup", acceptaddgroup},
                         {"rejectaddgroup", rejectaddgroup},
                         {"groupchat", groupchat},
                         {"loginout", loginout}};

// 聊天页面程序
void mainMenu(int clientfd) {
  help();

  char buffer[1024] = {0};
  while (isMainMenuRuning) {
    std::cin.getline(buffer, 1024);
    std::string commandbuf(buffer);
    std::string command; // 存储命令
    int idx = commandbuf.find(":");
    if (idx == -1) {
      command = commandbuf;
    } else {
      command = commandbuf.substr(0, idx); // 参数是开始位置和截取长度
    }

    auto it = commandHandlerMap.find(command);
    if (it == commandHandlerMap.end()) {
      std::cerr << "invalid input command!\n";
      continue;
    }

    // 调用命令的事件处理回调
    it->second(clientfd, commandbuf.substr(idx + 1, commandbuf.size() - idx));
  }
}

// “help” command handler
void help(int, std::string) {
  std::cout << "\n";
  std::cout << "show command list >>>\n";
  for (auto &p : commandMap) {
    std::cout << p.first << ":" << p.second << "\n";
  }
  std::cout << "\n";
}

// “addfriend” command handler addfriend:friendid
void addfriend(int clientfd, std::string str) {
  int friendid = atoi(str.c_str());
  nlohmann::json js;
  js["msgid"] = EnMsgType::ADD_FRIEND_REQUEST;
  js["userid"] = g_currentUser.getId();
  js["friendid"] = friendid;

  std::string buffer = js.dump() + "\n";

  int len = send(clientfd, buffer.c_str(), buffer.length(), 0);
  if (len == -1) {
    std::cerr << "send add friend msg error:" << buffer << "\n";
  }
}

// “acceptfriend” command handler acceptaddfriend:friendid
void acceptaddfriend(int clientfd, std::string str) {
  int requestUserId = atoi(str.c_str());
  nlohmann::json js;
  js["msgid"] = EnMsgType::ADD_FRIEND_HANDLE;
  js["userid"] = requestUserId;           // 之前发请求的 A
  js["friendid"] = g_currentUser.getId(); // 当前处理请求的 B
  js["action"] = "accept";

  std::string buffer = js.dump() + "\n";
  send(clientfd, buffer.c_str(), buffer.length(), 0);
}

// “rejectfriend” command handler rejectaddfriend:friendid
void rejectaddfriend(int clientfd, std::string str) {
  int requestUserId = atoi(str.c_str());
  nlohmann::json js;
  js["msgid"] = EnMsgType::ADD_FRIEND_HANDLE;
  js["userid"] = requestUserId;           // 之前发请求的 A
  js["friendid"] = g_currentUser.getId(); // 当前处理请求的 B
  js["action"] = "refuse";

  std::string buffer = js.dump() + "\n";
  send(clientfd, buffer.c_str(), buffer.length(), 0);
}

// 显示当前登录成功用户的基本信息
void showCurrentUesrInfo() {
  std::cout << "===================== login user ===================" << "\n";
  std::cout << "current login user => id：" << g_currentUser.getId()
            << " name：" << g_currentUser.getName() << "\n ";
  std::cout << "--------------------- friend list ------------------" << "\n";
  // 判断好友列表是否为空
  if (!g_currentUserFriendList.empty()) {
    for (User &user : g_currentUserFriendList) {
      std::cout << user.getId() << " " << user.getName() << " "
                << user.getState() << "\n";
    }
  }
  std::cout << "--------------------- group list ------------------" << "\n";
  // 判断群组列表是否为空
  if (!g_currentUserGroupList.empty()) {
    // 打印用户所在的群组
    for (Group &group : g_currentUserGroupList) {
      std::cout << "groupid:" << group.getId() << " groupname:" << " "
                << group.getName() << " " << " groupdesc:" << group.getDesc()
                << "\n";
      // 打印群组中的成员
      for (GroupUser &user : group.getUsers()) {
        std::cout << user.getId() << " " << user.getName() << " "
                  << user.getRole() << "\n";
      }
    }
  }
  std::cout << "====================================================" << "\n";
}

// “chat” command handler chat:friend:msg
void chat(int clientfd, std::string str) {
  int idx = str.find(":");
  if (idx == -1) {
    std::cerr << "chat command invalid!\n";
    return;
  }

  int friendid = atoi(str.substr(0, idx).c_str());
  std::string message = str.substr(idx + 1, str.size() - idx);

  nlohmann::json js;
  js["msgid"] = EnMsgType::ONE_CHAT_MSG;
  js["userid"] = g_currentUser.getId();
  js["name"] = g_currentUser.getName();
  js["to"] = friendid;
  js["msg"] = message;
  js["time"] = getCurrentTime();

  std::string buffer = js.dump() + "\n";

  if (send(clientfd, buffer.c_str(), buffer.length(), 0) == -1) {
    std::cerr << "send chat msg error:" << buffer << "\n";
  }
}

// “creategroup” command handler creategroup:groupname:groupdesc
void creategroup(int clientfd, std::string str) {
  int idx = str.find(":");
  if (idx == -1) {
    std::cerr << "creategroup command invalid!\n";
    return;
  }

  std::string name = str.substr(0, idx);
  std::string desc = str.substr(idx + 1, str.size() - idx);

  nlohmann::json js;
  js["msgid"] = EnMsgType::CREATE_GROUP_MSG;
  js["userid"] = g_currentUser.getId();
  js["groupname"] = name;
  js["groupdesc"] = desc;

  std::string buffer = js.dump() + "\n";

  if (send(clientfd, buffer.c_str(), buffer.length(), 0) == -1) {
    std::cerr << "send create group msg error:" << buffer << "\n";
  }
}

// “addgroup” command handler addgroup:groupid
void addgroup(int clientfd, std::string str) {
  int groupid = atoi(str.c_str());

  nlohmann::json js;
  js["msgid"] = EnMsgType::ADD_GROUP_MSG;
  js["userid"] = g_currentUser.getId();
  js["groupid"] = groupid;

  std::string buffer = js.dump() + "\n";

  if (send(clientfd, buffer.c_str(), buffer.length(), 0) == -1) {
    std::cerr << "send add group msg error:" << buffer << "\n";
  }
}

// acceptaddgroup command handler acceptaddgroup:userid:groupid
void acceptaddgroup(int clientfd, std::string str) {
  int idx = str.find(":");
  if (idx == -1) {
    std::cerr << "acceptaddgroup msg invalid!\n";
    return;
  }

  int userid = atoi(str.substr(0, idx).c_str());
  int groupid = atoi(str.substr(idx + 1, str.size() - idx).c_str());

  nlohmann::json js;
  js["msgid"] = EnMsgType::ADD_GROUP_HANDLE;
  js["userid"] = userid;
  js["groupid"] = groupid;
  js["action"] = "accept";

  std::string buffer = js.dump() + "\n";

  if (send(clientfd, buffer.c_str(), buffer.length(), 0) == -1) {
    std::cerr << "send add group msg error:" << buffer << "\n";
  }
}

// rejectaddgroup command handler rejectaddgroup:userid:groupid
void rejectaddgroup(int clientfd, std::string str) {
  int idx = str.find(":");
  if (idx == -1) {
    std::cerr << "acceptaddgroup msg invalid!\n";
    return;
  }

  int userid = atoi(str.substr(0, idx).c_str());
  int groupid = atoi(str.substr(idx + 1, str.size() - idx).c_str());

  nlohmann::json js;
  js["msgid"] = EnMsgType::ADD_GROUP_HANDLE;
  js["userid"] = userid;
  js["groupid"] = groupid;
  js["action"] = "reject";

  std::string buffer = js.dump() + "\n";

  if (send(clientfd, buffer.c_str(), buffer.length(), 0) == -1) {
    std::cerr << "send add group msg error:" << buffer << "\n";
  }
}

// “groupchat” command handler groupchat:groupid:message
void groupchat(int clientfd, std::string str) {
  int idx = str.find(":");
  if (idx == -1) {
    std::cerr << "groupchat command invalid!\n";
    return;
  }

  int groupid = atoi(str.substr(0, idx).c_str());
  std::string message = str.substr(idx + 1, str.size() - idx);

  nlohmann::json js;
  js["msgid"] = EnMsgType::GROUP_CHAT_MSG;
  js["userid"] = g_currentUser.getId();
  js["name"] = g_currentUser.getName();
  js["groupid"] = groupid;
  js["msg"] = message;
  js["time"] = getCurrentTime();

  std::string buffer = js.dump() + "\n";

  if (send(clientfd, buffer.c_str(), buffer.length(), 0) == -1) {
    std::cerr << "send group chat msg error:" << buffer << "\n";
  }
}

// “loginout” command handler
void loginout(int clientfd, std::string str) {
  nlohmann::json js;
  js["msgid"] = EnMsgType::LOGINOUT_MSG;
  js["userid"] = g_currentUser.getId();

  std::string buffer = js.dump() + "\n";

  if (send(clientfd, buffer.c_str(), buffer.length(), 0) == -1) {
    std::cerr << "send loginout msg error:" << buffer << "\n";
  } else {
    isMainMenuRuning = false;
  }
}

// 获取系统时间（聊天信息需要添加时间信息）
std::string getCurrentTime() {
  auto tt =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  struct tm *ptm = localtime(&tt);
  char date[60] = {0};
  sprintf(date, "%d-%02d-%02d %02d:%02d:%02d", (int)ptm->tm_year + 1900,
          (int)ptm->tm_mon + 1, (int)ptm->tm_mday, (int)ptm->tm_hour,
          (int)ptm->tm_min, (int)ptm->tm_sec);
  return std::string(date);
}
