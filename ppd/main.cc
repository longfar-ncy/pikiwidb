/*
 * Copyright (c) 2023-present, OpenAtom Foundation, Inc.  All rights reserved.
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include "pd_service.h"

int main(int argc, char* argv[]) {
  brpc::Server server;

  PlacementDriverServiceImpl service;
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