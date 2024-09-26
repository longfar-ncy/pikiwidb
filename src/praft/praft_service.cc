/*
 * Copyright (c) 2024-present, Qihoo, Inc.  All rights reserved.
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include "praft_service.h"

#include "fmt/format.h"
#include "store.h"

namespace pikiwidb {
void PRaftServiceImpl::AddNode(::google::protobuf::RpcController* controller, const ::pikiwidb::NodeAddRequest* request,
                               ::pikiwidb::NodeAddResponse* response, ::google::protobuf::Closure* done) {
  brpc::ClosureGuard done_guard(done);
  auto groupid = request->groupid();
  auto db_ptr = PSTORE.GetDBByGroupID(groupid);
  auto praft_ptr = db_ptr->GetPRaft();
  auto end_point = request->endpoint();
  auto index = request->index();
  auto role = request->role();
  auto status = praft_ptr->AddPeer(end_point, index);
  if (!status.ok()) {
    response->set_success(false);
    return;
  }
  response->set_success(true);
}
}  // namespace pikiwidb