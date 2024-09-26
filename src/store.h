// Copyright (c) 2023-present, OpenAtom Foundation, Inc.  All rights reserved.
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. An additional grant
// of patent rights can be found in the PATENTS file in the same directory

/*
  Connects external commands and coordinates the operation
  of RocksDB.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include "praft/praft.h"
#define GLOG_NO_ABBREVIATED_SEVERITIES

#include <memory>
#include <vector>

#include "brpc/server.h"
#include "butil/endpoint.h"

#include "db.h"

namespace pikiwidb {

enum TaskType { kCheckpoint = 0, kLoadDBFromCheckpoint, kEmpty };

enum TaskArg {
  kCheckpointPath = 0,
};

constexpr const char* ErrTypeMessage = "WRONGTYPE";

struct TaskContext {
  TaskType type = kEmpty;
  int db = -1;
  std::map<TaskArg, std::string> args;
  bool sync = false;
  TaskContext() = delete;
  TaskContext(TaskType t, bool s = false) : type(t), sync(s) {}
  TaskContext(TaskType t, int32_t d, bool s = false) : type(t), db(d), sync(s) {}
  TaskContext(TaskType t, int32_t d, const std::map<TaskArg, std::string>& a, bool s = false)
      : type(t), db(d), args(a), sync(s) {}
};

using TasksVector = std::vector<TaskContext>;

class PStore {
 public:
  static PStore& Instance();

  PStore(const PStore&) = delete;
  void operator=(const PStore&) = delete;
  ~PStore();

  void Init(int db_number);

  std::unique_ptr<DB>& GetBackend(int32_t index) { return backends_[index]; };

  void HandleTaskSpecificDB(const TasksVector& tasks);

  int GetDBNumber() const { return db_number_; }
  brpc::Server* GetRpcServer() const { return rpc_server_.get(); }
  const butil::EndPoint& GetEndPoint() const { return endpoint_; }
  void AddRegion(const std::string& group_id, uint32_t dbno) {
    assert(!db_map_.contains(group_id));
    db_map_.emplace(group_id, dbno);
  }
  DB* GetDBByGroupID(const std::string& group_id) const {
    auto it = db_map_.find(group_id);
    if (it == db_map_.end()) {
      return nullptr;
    }
    return backends_[it->second].get();
  }

 private:
  PStore() = default;

  int db_number_ = 0;
  std::vector<std::unique_ptr<DB>> backends_;
  butil::EndPoint endpoint_;
  std::unique_ptr<brpc::Server> rpc_server_{std::make_unique<brpc::Server>()};
  std::unordered_map<std::string, uint32_t> db_map_;
};

#define PSTORE PStore::Instance()

}  // namespace pikiwidb
