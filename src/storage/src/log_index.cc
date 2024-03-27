#include "log_index.h"

#include <cinttypes>

#include "redis.h"

namespace storage {

rocksdb::Status storage::LogIndexOfCF::Init(Redis *db, size_t cf_num) {
  for (int i = 0; i < cf_num; i++) {
    rocksdb::TablePropertiesCollection collection;
    auto s = db->GetDB()->GetPropertiesOfAllTables(db->GetColumnFamilyHandles()[i], &collection);
    if (!s.ok()) {
      return s;
    }
    LogIndex max_applied_log_index{};
    LogIndex max_flushed_log_index{};
    for (const auto &[_, props] : collection) {
      assert(props->column_family_id == i);
      auto res = LogIndexTablePropertiesCollector::ReadStatsFromTableProps(props);
      if (res.has_value()) {
        max_applied_log_index = std::max(max_applied_log_index, res->GetAppliedLogIndex());
        max_flushed_log_index = std::max(max_flushed_log_index, res->GetAppliedLogIndex());
      }
    }
    cf_[i].applied_log_index.store(max_applied_log_index);
    cf_[i].flushed_log_index.store(max_flushed_log_index);
  }
  return Status::OK();
}

std::optional<LogIndexAndSequencePair> storage::LogIndexTablePropertiesCollector::ReadStatsFromTableProps(
    const std::shared_ptr<const rocksdb::TableProperties> &table_props) {
  const auto &user_properties = table_props->user_collected_properties;
  const auto it = user_properties.find(kPropertyName.data());
  if (it == user_properties.end()) {
    return std::nullopt;
  }
  std::string s = it->second;
  LogIndex applied_log_index;
  SequenceNumber largest_seqno;
  auto res = sscanf(s.c_str(), "%" PRIi64 "/%" PRIu64 "", &applied_log_index, &largest_seqno);
  assert(res == 2);

  LogIndexAndSequencePair p(applied_log_index, largest_seqno);
  return p;
}

LogIndex LogIndexOfCF::GetSmallestLogIndex(std::function<LogIndex(const LogIndexPair &)> &&f) const {
  auto smallest_log_index = std::numeric_limits<LogIndex>::max();
  for (const auto &it : cf_) {
    smallest_log_index = std::min(f(it), smallest_log_index);
  }
  return smallest_log_index;
}

LogIndex LogIndexAndSequenceCollector::FindAppliedLogIndex(SequenceNumber seqno) const {
  if (seqno == 0) {
    return 0;
  }

  std::lock_guard<std::mutex> guard(mutex_);

  // use skip list to find the best iterator for search seqno
  // for (const auto &s : skip_list_) {
  //   if (seqno >= s.GetSequenceNumber()) {
  //     it = s.GetIterator();
  //   } else {
  //     break;
  //   }
  // }

  LogIndex applied_log_index = 0;
  auto it = list_.begin();
  for (auto it = list_.begin(); it != list_.end() && it->GetSequenceNumber() <= seqno; it++) {
    applied_log_index = it->GetAppliedLogIndex();
  }
  return applied_log_index;
}

void LogIndexAndSequenceCollector::Update(LogIndex smallest_applied_log_index, SequenceNumber smallest_flush_seqno) {
  /*
    If step length > 1, log index is sampled and sacrifice precision to save memory usage.
    It means that extra applied log may be applied again on start stage.
  */
  if ((smallest_applied_log_index & step_length_mask_) == 0) {
    std::lock_guard<std::mutex> guard(mutex_);
    list_.emplace_back(smallest_applied_log_index, smallest_flush_seqno);

    // if (applied_log_index & skip_length_mask_ == 0) {
    //   PairAndIterator s(pair, std::prev(list_.end()));
    //   skip_list_.emplace_back(s);
    // }
  }
}

}  // namespace storage
