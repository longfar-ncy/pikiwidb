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
    client->SetRes(CmdRes::kErrOther, "Unknown error");
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
    client->SetRes(CmdRes::kErrOther, "Unknown error");
    return;
  }
  client->AppendContent(reply.ReadStringWithoutCRLF());
}

}  // namespace pikiwidb