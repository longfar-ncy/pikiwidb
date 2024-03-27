#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <list>
#include <mutex>
#include <optional>
#include <string_view>
#include <utility>

#include "fmt/core.h"
#include "rocksdb/db.h"
#include "rocksdb/listener.h"
#include "rocksdb/table_properties.h"
#include "rocksdb/types.h"
#include "storage/storage_define.h"

namespace storage {

using LogIndex = int64_t;
using rocksdb::SequenceNumber;
class Redis;

class LogIndexAndSequencePair {
 public:
  LogIndexAndSequencePair(LogIndex applied_log_index, SequenceNumber seqno)
      : applied_log_index_(applied_log_index), seqno_(seqno) {}

  void SetAppliedLogIndex(int64_t applied_log_index) { applied_log_index_ = applied_log_index; }
  void SetSequenceNumber(SequenceNumber seqno) { seqno_ = seqno; }

  LogIndex GetAppliedLogIndex() const { return applied_log_index_; }
  SequenceNumber GetSequenceNumber() const { return seqno_; }

 private:
  LogIndex applied_log_index_ = 0;
  SequenceNumber seqno_ = 0;
};

class LogIndexOfCF {
  struct LogIndexPair {
    std::atomic<LogIndex> applied_log_index = 0;  // newest record in memtable.
    std::atomic<LogIndex> flushed_log_index = 0;  // newest rocord in sst file.
  };

 public:
  // Read the largest log index of each column family from all sst files
  rocksdb::Status Init(Redis *db);

  LogIndex GetSmallestAppliedLogIndex() const {
    return GetSmallestLogIndex([](const LogIndexPair &p) { return p.applied_log_index.load(); });
  }
  LogIndex GetSmallestFlushedLogIndex() const {
    return GetSmallestLogIndex([](const LogIndexPair &p) { return p.flushed_log_index.load(); });
  }
  void SetFlushedLogIndex(size_t cf_id, LogIndex log_index) {
    cf_[cf_id].flushed_log_index = std::max(cf_[cf_id].flushed_log_index.load(), log_index);
  }

  // bool CheckIfApplyAndSet(size_t cf_id, LogIndex cur_log_index) {
  //   cf_[cf_id].applied_log_index = std::max(cf_[cf_id].applied_log_index.load(), cur_log_index);
  //   return cur_log_index == cf_[cf_id].applied_log_index.load();
  // }

 private:
  LogIndex GetSmallestLogIndex(std::function<LogIndex(const LogIndexPair &)> &&f) const;
  std::array<LogIndexPair, kColumnFamilyNum> cf_;
};

class LogIndexAndSequenceCollector {
 public:
  explicit LogIndexAndSequenceCollector(uint8_t step_length_bit = 0, uint8_t extra_skip_length_bit = 8) {
    step_length_mask_ = (1 << step_length_bit) - 1;
    skip_length_mask_ = (1 << (step_length_bit + extra_skip_length_bit)) - 1;
  }

  // find the index of log which contain seqno or before it
  LogIndex FindAppliedLogIndex(SequenceNumber seqno) const;

  // if there's a new pair, add it to list; otherwise, do nothing
  void Update(LogIndex smallest_applied_log_index, SequenceNumber smallest_flush_seqno);

  // purge out dated log index after memtable flushed.
  void Purge(LogIndex smallest_applied_log_index, LogIndex smallest_flushed_log_index) {
    std::lock_guard<std::mutex> guard(mutex_);
    Purge(list_, smallest_applied_log_index, smallest_flushed_log_index);
    // Purge(skip_list_, smallest_applied_log_index, smallest_flushed_log_index);
  }

 private:
  template <typename T>
  void Purge(std::list<T> list, LogIndex smallest_applied_log_index, LogIndex smallest_flushed_log_index) {
    // purge condition:
    // We found first pair is greater than both smallest_flushed_log_index and smallest_applied_log_index,
    // then we keep previous one, and purge everyone before previous one.
    while (list.size() >= 2) {
      auto cur = list.begin();
      auto next = std::next(cur);
      if (smallest_flushed_log_index > cur->GetAppliedLogIndex() &&
          smallest_applied_log_index > next->GetAppliedLogIndex()) {
        list.pop_front();
      } else {
        break;
      }
      ++cur;
    }
  }

 private:
  uint64_t step_length_mask_ = 0;
  uint64_t skip_length_mask_ = 0;
  mutable std::mutex mutex_;
  std::list<LogIndexAndSequencePair> list_;
  // class PairAndIterator {
  //  public:
  //   PairAndIterator() {}
  //   PairAndIterator(LogIndexAndSequencePair pair, decltype(list_)::iterator iter) : pair_(pair), iter_(iter) {}
  //   inline int64_t GetAppliedLogIndex() const { return pair_.GetAppliedLogIndex(); }
  //   inline SequenceNumber GetSequenceNumber() const { return pair_.GetSequenceNumber(); }
  //   inline decltype(list_)::iterator GetIterator() const { return iter_; }

  //  private:
  //   LogIndexAndSequencePair pair_;
  //   decltype(list_)::iterator iter_;
  // };
  // std::list<PairAndIterator> skip_list_;
};

class LogIndexTablePropertiesCollector : public rocksdb::TablePropertiesCollector {
 public:
  static constexpr std::string_view kPropertyName = "LargestLogIndex/LargestSequenceNumber";

  explicit LogIndexTablePropertiesCollector(const LogIndexAndSequenceCollector &collector) : collector_(collector) {}

  rocksdb::Status AddUserKey(const rocksdb::Slice &key, const rocksdb::Slice &value, rocksdb::EntryType type,
                             SequenceNumber seq, uint64_t file_size) override {
    largest_seqno_ = std::max(largest_seqno_, seq);
    return rocksdb::Status::OK();
  }
  rocksdb::Status Finish(rocksdb::UserCollectedProperties *properties) override {
    properties->insert(Materialize());
    return rocksdb::Status::OK();
  }
  const char *Name() const override { return "LogIndexTablePropertiesCollector"; }
  rocksdb::UserCollectedProperties GetReadableProperties() const override {
    return rocksdb::UserCollectedProperties{Materialize()};
  }

  static std::optional<LogIndexAndSequencePair> ReadStatsFromTableProps(
      const std::shared_ptr<const rocksdb::TableProperties> &table_props);

 private:
  std::pair<std::string, std::string> Materialize() const {
    if (0 == cache_) {
      cache_ = collector_.FindAppliedLogIndex(largest_seqno_);
    }
    return std::make_pair(static_cast<std::string>(kPropertyName), fmt::format("{}/{}", cache_, largest_seqno_));
  }

 private:
  const LogIndexAndSequenceCollector &collector_;
  SequenceNumber largest_seqno_ = 0;
  mutable LogIndex cache_{};
  // SequenceNumber smallest_seqno_ = 0;
};

class LogIndexTablePropertiesCollectorFactory : public rocksdb::TablePropertiesCollectorFactory {
 public:
  explicit LogIndexTablePropertiesCollectorFactory(const LogIndexAndSequenceCollector &collector)
      : collector_(collector) {}
  ~LogIndexTablePropertiesCollectorFactory() override = default;

  rocksdb::TablePropertiesCollector *CreateTablePropertiesCollector(
      [[maybe_unused]] rocksdb::TablePropertiesCollectorFactory::Context context) override {
    return new LogIndexTablePropertiesCollector(collector_);
  }
  const char *Name() const override { return "LogIndexTablePropertiesCollectorFactory"; }

 private:
  const LogIndexAndSequenceCollector &collector_;
};

class LogIndexAndSequenceCollectorPurger : public rocksdb::EventListener {
 public:
  explicit LogIndexAndSequenceCollectorPurger(LogIndexAndSequenceCollector *collector, LogIndexOfCF *cf)
      : collector_(collector), cf_(cf) {}

  void OnFlushCompleted(rocksdb::DB *db, const rocksdb::FlushJobInfo &flush_job_info) override {
    cf_->SetFlushedLogIndex(flush_job_info.cf_id, collector_->FindAppliedLogIndex(flush_job_info.largest_seqno));
    auto smallest_applied_log_index = cf_->GetSmallestAppliedLogIndex();
    auto smallest_flushed_log_index = cf_->GetSmallestFlushedLogIndex();
    collector_->Purge(smallest_applied_log_index, smallest_flushed_log_index);
  }

 private:
  LogIndexAndSequenceCollector *collector_;
  LogIndexOfCF *cf_;
};

}  // namespace storage
