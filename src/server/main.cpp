#include "../../include/server/chatserver.hpp"
#include "../../include/server/chatservice.hpp"
#include "../../include/server/logger.h"
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>

#ifndef CHATSERVER_LOG_DIR
#define CHATSERVER_LOG_DIR "."
#endif

void resetHandler(int) {
  ChatService::instance()->reset();
  exit(0);
}

int main(int argc, char **argv) {
  if (argc < 3) {
    std::cerr << "command invalid! example: ./ChatServer 127.0.0.1 6000"
              << "\n";
    exit(-1);
  }

  // 解析通过命令行参数传递的ip和port
  char *ip = argv[1];
  uint16_t port = atoi(argv[2]);

  if (!Logger::GetInstance().Init(CHATSERVER_LOG_DIR, "chatserver",
                                  std::to_string(port))) {
    std::cerr << "logger init failed! log dir: " << CHATSERVER_LOG_DIR << "\n";
    exit(EXIT_FAILURE);
  }

  ChatService::instance();

  std::signal(SIGINT, resetHandler);

  muduo::net::EventLoop loop;
  // 官方 muduo InetAddress(StringArg ip, uint16_t port) — ip 在前，port 在后
  muduo::net::InetAddress addr(ip, port);

  ChatServer server(&loop, addr, "ChatServer");

  server.start();
  LOG_INFO("ChatServer started at %s:%d\n", ip, port);
  loop.loop();
}
