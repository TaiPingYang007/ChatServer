#include "../../include/server/chatserver.hpp"
#include "../../include/server/chatservice.hpp"
#include <csignal>

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

  std::signal(SIGINT, resetHandler);

  EventLoop loop;
  InetAddress addr(port, ip);

  ChatServer server(&loop, addr, "ChatServer");

  server.start();
  loop.loop();
}