#pragma once

#include "proxy.pb.h"

class ProxyServiceImpl : public ProxyService {
 public:
  void RunCommand(::google::protobuf::RpcController* cntl, const pikiwidb::proxy::RunCommandRequest* request,
                  pikiwidb::proxy::RunCommandResponse* response, ::google::protobuf::Closure* done) override;
  void GetRouteInfo(::google::protobuf::RpcController* cntl, const pikiwidb::proxy::GetRouteInfoRequest* request,
                    pikiwidb::proxy::GetRouteInfoResponse* response, ::google::protobuf::Closure* done) override;

 private:
  std::string ExecuteCommand(const std::string& command);
};
