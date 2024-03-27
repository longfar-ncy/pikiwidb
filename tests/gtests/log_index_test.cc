#include <memory>
#include <string>

#include "gtest/gtest.h"

#include "praft/praft.h"
#include "pstd/pstd_string.h"
#include "rocksdb/db.h"
#include "rocksdb/listener.h"
#include "rocksdb/options.h"
#include "src/log_index.h"
#include "storage/storage.h"
#include "storage/util.h"

using namespace storage;  // NOLINT

TEST(TablePropertyTest, SimpleTest) {
  constexpr const char* kDbPath = "./log_index_test_db";
  rocksdb::Options options;
  options.create_if_missing = true;
  LogIndexAndSequenceCollector collector;
  options.table_properties_collector_factories.push_back(
      std::make_shared<LogIndexTablePropertiesCollectorFactory>(collector));
  rocksdb::DB* db{nullptr};
  auto s = rocksdb::DB::Open(options, kDbPath, &db);
  EXPECT_TRUE(s.ok());

  std::string key = "table-property-test";
  s = db->Put(rocksdb::WriteOptions(), key, key);
  EXPECT_TRUE(s.ok());
  std::string res;
  s = db->Get(rocksdb::ReadOptions(), key, &res);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(key, res);
  collector.Update(233333, db->GetLatestSequenceNumber());
  db->Flush(rocksdb::FlushOptions());

  rocksdb::TablePropertiesCollection properties;
  s = db->GetPropertiesOfAllTables(&properties);
  EXPECT_TRUE(s.ok());
  EXPECT_TRUE(properties.size() == 1);
  for (auto& [name, prop] : properties) {
    const auto& collector = prop->user_collected_properties;
    auto it = collector.find(static_cast<std::string>(LogIndexTablePropertiesCollector::kPropertyName));
    EXPECT_NE(it, collector.cend());
    EXPECT_EQ(it->second, "233333/" + std::to_string(db->GetLatestSequenceNumber()));
  }

  db->Close();
  DeleteFiles(kDbPath);
}

class LogIndexTest : public ::testing::Test {
 public:
  LogIndexTest() {
    if (access(db_path_.c_str(), F_OK) != 0) {
      mkdir(db_path_.c_str(), 0755);
    }
    options_.options.create_if_missing = true;
    auto cluster_id = pstd::RandomHexChars(RAFT_DBID_LEN);
    pikiwidb::PRaft::Instance().Init(cluster_id, false);
  }
  ~LogIndexTest() override { DeleteFiles(db_path_.c_str()); }

  std::string db_path_{"./test_db/log_index_test"};
  StorageOptions options_;
  Storage db_;
  uint32_t test_times_ = 100;
  std::string key_ = "log-index-test";
  std::string field_prefix_ = "field";
  std::string value_prefix_ = "value";
  rocksdb::WriteOptions write_options_;
  rocksdb::ReadOptions read_options_;
};

TEST_F(LogIndexTest, DoNothing) {}

// TEST_F(LogIndexTest, SimpleTest) {  // NOLINT
//   options_.is_use_raft = true;
//   auto s = db_.Open(options_, db_path_);
//   EXPECT_TRUE(s.ok());

//   auto& redis = db_.GetDBInstance(key_);
//   for (int i = 0; i < test_times_; i++) {
//     auto field = field_prefix_ + std::to_string(i);
//     auto value = value_prefix_ + std::to_string(i);
//     int32_t res{};
//     s = redis->HSet(key_, field, value, &res);
//     EXPECT_TRUE(s.ok());
//     EXPECT_EQ(1, res);

//     std::string get_res;
//     s = redis->HGet(key_, field, &get_res);
//     EXPECT_TRUE(s.ok());
//     EXPECT_EQ(value, get_res);
//   }
//   redis->GetDB()->Flush(rocksdb::FlushOptions(), redis->GetColumnFamilyHandle(kHashesMetaCF));
//   redis->GetDB()->Flush(rocksdb::FlushOptions(), redis->GetColumnFamilyHandle(kHashesDataCF));

//   rocksdb::TablePropertiesCollection properties;
//   s = redis->GetDB()->GetPropertiesOfAllTables(redis->GetColumnFamilyHandles()[kHashesMetaCF], &properties);
//   EXPECT_TRUE(s.ok());
//   EXPECT_TRUE(properties.size() == 1);
//   for (auto& [name, prop] : properties) {
//     const auto& collector = prop->user_collected_properties;
//     auto it = collector.find(LogIndexTablePropertiesCollector::kPropertyName_);
//     EXPECT_NE(it, collector.cend());
//     EXPECT_EQ(it->second, "99999999/" + std::to_string(redis->GetDB()->GetLatestSequenceNumber() - 1));
//   }

//   properties.clear();
//   s = redis->GetDB()->GetPropertiesOfAllTables(redis->GetColumnFamilyHandle(kHashesDataCF), &properties);
//   EXPECT_TRUE(s.ok());
//   EXPECT_TRUE(properties.size() == 1);
//   for (auto& [name, prop] : properties) {
//     const auto& collector = prop->user_collected_properties;
//     auto it = collector.find(LogIndexTablePropertiesCollector::kPropertyName_);
//     EXPECT_NE(it, collector.cend());
//     EXPECT_EQ(it->second, "99999999/" + std::to_string(redis->GetDB()->GetLatestSequenceNumber()));
//   }
// }
