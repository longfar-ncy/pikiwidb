#include "cmd_hash.h"

#include "command.h"

namespace pikiwidb {

HSetCmd::HSetCmd(const std::string& name, int16_t arity)
    : BaseCmd(name, arity, CmdFlagsWrite, AclCategoryWrite | AclCategoryHash) {}

bool HSetCmd::DoInitial(PClient* client) { return true; }

void HSetCmd::DoCmd(PClient* client) {
  UnboundedBuffer reply;
  std::vector<std::string> params(client->argv_.begin(), client->argv_.end());
  PError err = hset(params, &reply);
  if (err != PError_ok) {
    if (err == PError_notExist) {
      client->AppendString("");
    } else {
      client->SetRes(CmdRes::kErrOther, "hset unknown error");
    }
    return;
  }
  client->AppendContent(reply.ReadStringWithoutCRLF());
}

HGetCmd::HGetCmd(const std::string& name, int16_t arity)
    : BaseCmd(name, arity, CmdFlagsReadonly, AclCategoryRead | AclCategoryHash) {}

bool HGetCmd::DoInitial(PClient* client) { return true; }

void HGetCmd::DoCmd(PClient* client) {
  UnboundedBuffer reply;
  std::vector<std::string> params(client->argv_.begin(), client->argv_.end());
  PError err = hget(params, &reply);
  if (err != PError_ok) {
    if (err == PError_notExist) {
      client->AppendString("");
    } else {
      client->SetRes(CmdRes::kErrOther, "hget unknown error");
    }
    return;
  }
  client->AppendContent(reply.ReadStringWithoutCRLF());
}

HMSetCmd::HMSetCmd(const std::string& name, int16_t arity)
    : BaseCmd(name, arity, CmdFlagsWrite, AclCategoryWrite | AclCategoryHash) {}

bool HMSetCmd::DoInitial(PClient* client) { return true; }

void HMSetCmd::DoCmd(PClient* client) {
  UnboundedBuffer reply;
  std::vector<std::string> params(client->argv_.begin(), client->argv_.end());
  PError err = hmset(params, &reply);
  if (err != PError_ok) {
    if (err == PError_notExist) {
      client->AppendString("");
    } else {
      client->SetRes(CmdRes::kErrOther, "hmset unknown error");
    }
    return;
  }
  client->AppendContent(reply.ReadStringWithoutCRLF());
}

HMGetCmd::HMGetCmd(const std::string& name, int16_t arity)
    : BaseCmd(name, arity, CmdFlagsReadonly, AclCategoryRead | AclCategoryHash) {}

bool HMGetCmd::DoInitial(PClient* client) { return true; }

void HMGetCmd::DoCmd(PClient* client) {
  UnboundedBuffer reply;
  std::vector<std::string> params(client->argv_.begin(), client->argv_.end());
  PError err = hmget(params, &reply);
  if (err != PError_ok) {
    if (err == PError_notExist) {
      client->AppendString("");
    } else {
      client->SetRes(CmdRes::kErrOther, "hmget unknown error");
    }
    return;
  }
  client->AppendContent(reply.ReadStringWithoutCRLF());
}

}  // namespace pikiwidb