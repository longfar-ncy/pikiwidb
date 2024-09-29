#include "proxy_service.h"

int main(int argc, char* argv[]) {
  brpc::Server server;

  ProxyServiceImpl service;
  if (server.AddService(&service, brpc::SERVER_OWNS_SERVICE) != 0) {
    fprintf(stderr, "Fail to add service!\n");
    return -1;
  }

  // 启动服务
  if (server.Start(8080, nullptr) != 0) {
    fprintf(stderr, "Fail to start server!\n");
    return -1;
  }

  server.RunUntilAskedToQuit();
  return 0;
}