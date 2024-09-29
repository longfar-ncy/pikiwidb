#include "proxy_service.h"

namespace pikiwidb::proxy {
void ProxyServiceImpl::RunCommand(::google::protobuf::RpcController* cntl,
                                  const pikiwidb::proxy::RunCommandRequest* request,
                                  pikiwidb::proxy::RunCommandResponse* response, ::google::protobuf::Closure* done) {
  std::string command = request->command();
  std::string output = ExecuteCommand(command);

  response->set_output(output);

  done->Run();
}
void ProxyServiceImpl::GetRouteINfo(::google::protobuf::RpcController* cntl,
                                    const pikiwidb::proxy::GetRouteInfoRequest* request,
                                    pikiwidb::proxy::GetRouteInfoResponse* response,
                                    ::google::protobuf::Closure* done) {
}

std::string ProxyServiceImpl::ExecuteCommand(const std::string& command) {
  std::array<char, 128> buffer;
  std::string result;

  // 使用 popen 执行命令
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
  if (!pipe) {
    return "popen() failed!";
  }
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }
  return result;
}

}  // namespace pikiwidb::proxy