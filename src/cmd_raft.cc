// Copyright (c) 2023-present, OpenAtom Foundation, Inc.  All rights reserved.
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. An additional grant
// of patent rights can be found in the PATENTS file in the same directory

/*
  A set of instructions and functions related to Raft.

  Defined a set of functions and instructions related to the
  implementation of Raft, which is key to the distributed cluster implementation of PikiwiDB.
 */

#include "cmd_raft.h"

#include <cstdint>
#include <optional>
#include <string>

#include "brpc/channel.h"
#include "fmt/format.h"

#include "praft/praft.h"
#include "pstd/log.h"
#include "pstd/pstd_string.h"

#include "client.h"
#include "config.h"
#include "pikiwidb.h"
#include "praft.pb.h"
#include "replication.h"
#include "store.h"

namespace pikiwidb {

RaftNodeCmd::RaftNodeCmd(const std::string& name, int16_t arity)
    : BaseCmd(name, arity, kCmdFlagsRaft, kAclCategoryRaft) {}

bool RaftNodeCmd::DoInitial(PClient* client) {
  auto cmd = client->argv_[1];
  pstd::StringToUpper(cmd);

  if (cmd != kAddCmd && cmd != kRemoveCmd && cmd != kDoSnapshot) {
    client->SetRes(CmdRes::kErrOther, "RAFT.NODE supports ADD / REMOVE / DOSNAPSHOT only");
    return false;
  }
  group_id_ = client->argv_[2];
  praft_ = PSTORE.GetBackend(client->GetCurrentDB())->GetPRaft();
  return true;
}

void RaftNodeCmd::DoCmd(PClient* client) {
  assert(praft_);
  auto cmd = client->argv_[1];
  pstd::StringToUpper(cmd);
  if (cmd == kAddCmd) {
    DoCmdAdd(client);
  } else if (cmd == kRemoveCmd) {
    DoCmdRemove(client);
  } else if (cmd == kDoSnapshot) {
    assert(0);  // TODO(longfar): add group id in arguments
    DoCmdSnapshot(client);
  } else {
    client->SetRes(CmdRes::kErrOther, "RAFT.NODE supports ADD / REMOVE / DOSNAPSHOT only");
  }
}

void RaftNodeCmd::DoCmdAdd(PClient* client) {
  DEBUG("Received RAFT.NODE ADD cmd from {}", client->PeerIP());
  auto db = PSTORE.GetDBByGroupID(group_id_);
  assert(db);
  auto praft = db->GetPRaft();
  // Check whether it is a leader. If it is not a leader, return the leader information
  if (!praft->IsLeader()) {
    client->SetRes(CmdRes::kWrongLeader, praft_->GetLeaderID());
    return;
  }

  if (client->argv_.size() != 4) {
    client->SetRes(CmdRes::kWrongNum, client->CmdName());
    return;
  }

  // RedisRaft has nodeid, but in Braft, NodeId is IP:Port.
  // So we do not need to parse and use nodeid like redis;
  auto s = praft->AddPeer(client->argv_[3]);
  if (s.ok()) {
    client->SetRes(CmdRes::kOK);
  } else {
    client->SetRes(CmdRes::kErrOther, fmt::format("Failed to add peer: {}", s.error_str()));
  }
}

void RaftNodeCmd::DoCmdRemove(PClient* client) {
  // If the node has been initialized, it needs to close the previous initialization and rejoin the other group
  if (!praft_->IsInitialized()) {
    client->SetRes(CmdRes::kErrOther, "Don't already cluster member");
    return;
  }

  if (client->argv_.size() != 3) {
    client->SetRes(CmdRes::kWrongNum, client->CmdName());
    return;
  }

  // Check whether it is a leader. If it is not a leader, send remove request to leader
  if (!praft_->IsLeader()) {
    // Get the leader information
    braft::PeerId leader_peer_id(praft_->GetLeaderID());
    // @todo There will be an unreasonable address, need to consider how to deal with it
    if (leader_peer_id.is_empty()) {
      client->SetRes(CmdRes::kErrOther,
                     "The leader address of the cluster is incorrect, try again or delete the node from another node");
      return;
    }

    // Connect target
    std::string peer_ip = butil::ip2str(leader_peer_id.addr.ip).c_str();
    auto port = leader_peer_id.addr.port - pikiwidb::g_config.raft_port_offset;
    auto peer_id = client->argv_[2];
    auto ret =
        praft_->GetClusterCmdCtx().Set(ClusterCmdType::kRemove, client, std::move(peer_ip), port, std::move(peer_id));
    if (!ret) {  // other clients have removed
      return client->SetRes(CmdRes::kErrOther, "Other clients have removed");
    }
    praft_->GetClusterCmdCtx().ConnectTargetNode();
    INFO("Sent remove request to leader successfully");

    // Not reply any message here, we will reply after the connection is established.
    client->Clear();
    return;
  }

  auto s = praft_->RemovePeer(client->argv_[2]);
  if (s.ok()) {
    client->SetRes(CmdRes::kOK);
  } else {
    client->SetRes(CmdRes::kErrOther, fmt::format("Failed to remove peer: {}", s.error_str()));
  }
}

void RaftNodeCmd::DoCmdSnapshot(PClient* client) {
  auto s = praft_->DoSnapshot();
  if (s.ok()) {
    client->SetRes(CmdRes::kOK);
  }
}

RaftClusterCmd::RaftClusterCmd(const std::string& name, int16_t arity)
    : BaseCmd(name, arity, kCmdFlagsRaft, kAclCategoryRaft) {}

bool RaftClusterCmd::DoInitial(PClient* client) {
  auto cmd = client->argv_[1];
  pstd::StringToUpper(cmd);
  if (cmd != kInitCmd && cmd != kJoinCmd) {
    client->SetRes(CmdRes::kErrOther, "RAFT.CLUSTER supports INIT/JOIN only");
    return false;
  }

  praft_ = PSTORE.GetBackend(client->GetCurrentDB())->GetPRaft();
  return true;
}

void RaftClusterCmd::DoCmd(PClient* client) {
  assert(praft_);
  if (praft_->IsInitialized()) {
    return client->SetRes(CmdRes::kErrOther, "Already cluster member");
  }

  auto cmd = client->argv_[1];
  pstd::StringToUpper(cmd);
  if (cmd == kInitCmd) {
    DoCmdInit(client);
  } else {
    DoCmdJoin(client);
  }
}

void RaftClusterCmd::DoCmdInit(PClient* client) {
  if (client->argv_.size() != 2 && client->argv_.size() != 3) {
    return client->SetRes(CmdRes::kWrongNum, client->CmdName());
  }

  std::string group_id;
  if (client->argv_.size() == 3) {
    group_id = client->argv_[2];
    if (group_id.size() != RAFT_GROUPID_LEN) {
      return client->SetRes(CmdRes::kInvalidParameter,
                            "Cluster id must be " + std::to_string(RAFT_GROUPID_LEN) + " characters");
    }
  } else {
    group_id = pstd::RandomHexChars(RAFT_GROUPID_LEN);
  }

  auto add_region_success = PSTORE.AddRegion(group_id, client->GetCurrentDB());
  if (add_region_success) {
    auto s = praft_->Init(group_id, false);
    if (!s.ok()) {
      PSTORE.RemoveRegion(group_id);
      ClearPaftCtx();
      return client->SetRes(CmdRes::kErrOther, fmt::format("Failed to init raft node: {}", s.error_str()));
    }
    client->SetLineString(fmt::format("+OK {}", group_id));
  } else {
    client->SetRes(CmdRes::kErrOther, fmt::format("The current GroupID {} already exists", group_id));
  }
}

static inline std::optional<std::pair<std::string, int32_t>> GetIpAndPortFromEndPoint(const std::string& endpoint) {
  auto pos = endpoint.find(':');
  if (pos == std::string::npos) {
    return std::nullopt;
  }

  int32_t ret = 0;
  pstd::String2int(endpoint.substr(pos + 1), &ret);
  return {{endpoint.substr(0, pos), ret}};
}

void RaftClusterCmd::DoCmdJoin(PClient* client) {
  assert(client->argv_.size() == 4);
  auto group_id = client->argv_[2];
  auto addr = client->argv_[3];
  butil::EndPoint endpoint;
  if (0 != butil::str2endpoint(addr.c_str(), &endpoint)) {
    ERROR("Wrong endpoint format: {}", addr);
    return client->SetRes(CmdRes::kErrOther, "Wrong endpoint format");
  }
  endpoint.port += g_config.raft_port_offset;

  if (group_id.size() != RAFT_GROUPID_LEN) {
    return client->SetRes(CmdRes::kInvalidParameter,
                          "Cluster id must be " + std::to_string(RAFT_GROUPID_LEN) + " characters");
  }

  auto add_region_success = PSTORE.AddRegion(group_id, client->GetCurrentDB());
  if (add_region_success) {
    auto s = praft_->Init(group_id, false);
    if (!s.ok()) {
      PSTORE.RemoveRegion(group_id);
      ClearPaftCtx();
      return client->SetRes(CmdRes::kErrOther, fmt::format("Failed to init raft node: {}", s.error_str()));
    }
  } else {
    client->SetRes(CmdRes::kErrOther, fmt::format("The current GroupID {} already exists", group_id));
  }

  brpc::ChannelOptions options;
  options.connection_type = brpc::CONNECTION_TYPE_SINGLE;
  options.max_retry = 0;
  options.connect_timeout_ms = kChannelTimeoutMS;

  brpc::Channel add_node_channel;
  if (0 != add_node_channel.Init(endpoint, &options)) {
    PSTORE.RemoveRegion(group_id);
    ClearPaftCtx();
    ERROR("Fail to init add_node_channel to praft service!");
    client->SetRes(CmdRes::kErrOther, "Fail to init add_node_channel.");
    return;
  }

  brpc::Controller cntl;
  NodeAddRequest request;
  NodeAddResponse response;
  auto end_point = butil::endpoint2str(PSTORE.GetEndPoint()).c_str();
  request.set_groupid(group_id);
  request.set_endpoint(std::string(end_point));
  request.set_index(client->GetCurrentDB());
  request.set_role(0);
  PRaftService_Stub stub(&add_node_channel);
  stub.AddNode(&cntl, &request, &response, NULL);

  if (cntl.Failed()) {
    PSTORE.RemoveRegion(group_id);
    ClearPaftCtx();
    ERROR("Fail to send add node rpc to target server {}", addr);
    client->SetRes(CmdRes::kErrOther, "Failed to send add node rpc");
    return;
  }
  if (response.success()) {
    client->SetRes(CmdRes::kOK, "Add Node Success");
    return;
  }
  PSTORE.RemoveRegion(group_id);
  ClearPaftCtx();
  ERROR("Add node request return false");
  client->SetRes(CmdRes::kErrOther, "Failed to Add Node");
}

void RaftClusterCmd::ClearPaftCtx() {
  assert(praft_);
  praft_->ShutDown();
  praft_->Join();
  praft_->Clear();
  praft_ = nullptr;
}
}  // namespace pikiwidb
