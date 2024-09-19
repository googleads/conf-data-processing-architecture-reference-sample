/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cc/publisher_list_generator/id_encryptor/src/random_id_encryptor.h"

#include <gtest/gtest.h>

#include <string>
#include <thread>

#include "absl/container/flat_hash_set.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "cc/core/async_executor/src/async_executor.h"
#include "cc/core/common/concurrent_queue/src/concurrent_queue.h"
#include "cc/core/common/uuid/src/uuid.h"
#include "cc/core/test/utils/conditional_wait.h"
#include "cc/public/core/interface/execution_result.h"
#include "cc/public/core/test/interface/execution_result_matchers.h"

using google::scp::core::AsyncExecutor;
using google::scp::core::ConsumerStreamingContext;
using google::scp::core::ProducerStreamingContext;
using google::scp::core::common::ConcurrentQueue;
using google::scp::core::common::Uuid;
using google::scp::core::test::IsSuccessfulAndHolds;
using google::scp::core::test::WaitUntil;
using std::atomic_bool;
using std::make_shared;
using std::move;
using std::shared_ptr;
using std::string;
using std::thread;
using testing::Values;

namespace google::pair::publisher_list_generator::test {

class RandomIdEncryptorForTest : public RandomIdEncryptor {
 public:
  using RandomIdEncryptor::RandomIdEncryptor;

  ConcurrentQueue<PlaintextAndEncrypted>& GetEncryptedIdsQueue() {
    return GetEncryptedIds();
  }
};

class RandomIdEncryptorTest : public testing::TestWithParam<int> {
 protected:
  RandomIdEncryptorTest()
      : cpu_async_executor_(make_shared<AsyncExecutor>(5, 1000000)),
        encryptor_(cpu_async_executor_) {
    EXPECT_SUCCESS(cpu_async_executor_->Init());
    EXPECT_SUCCESS(cpu_async_executor_->Run());
  }

  ~RandomIdEncryptorTest() { EXPECT_SUCCESS(cpu_async_executor_->Stop()); }

  int GetNumIdsToEncrypt() const { return GetParam(); }

  /**
   * @brief Gets a string roughly in the form of an email address between 3
   * and 320 characters long.
   */
  string GetRandomString() {
    constexpr char kAllowedChars[] =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.@";
    int length = absl::Uniform(bitgen_, 3, 320);
    string s;
    s.reserve(length);
    for (int i = 0; i < length; i++) {
      auto index =
          absl::Uniform(bitgen_, static_cast<size_t>(0), sizeof(kAllowedChars));
      s += kAllowedChars[index];
    }
    return s;
  }
  absl::BitGen bitgen_;

  shared_ptr<AsyncExecutor> cpu_async_executor_;
  RandomIdEncryptorForTest encryptor_;
};

TEST_P(RandomIdEncryptorTest, EncryptsPlaintext) {
  atomic_bool finish_called{false};
  ProducerStreamingContext<string, EncryptResult> encrypt_context(
      GetNumIdsToEncrypt());
  encrypt_context.callback = [&finish_called](auto& context) {
    EXPECT_SUCCESS(context.result);
    finish_called = true;
  };
  thread producer_thread([this, &encrypt_context]() {
    for (int i = 0; i < GetNumIdsToEncrypt(); i++) {
      EXPECT_SUCCESS(encrypt_context.TryPushRequest(GetRandomString()));
    }
    encrypt_context.MarkDone();
  });
  encryptor_.Encrypt(encrypt_context);
  absl::flat_hash_set<Uuid> found_ids;
  auto& encrypted_id_queue = encryptor_.GetEncryptedIdsQueue();

  while (!finish_called.load()) {
    RandomIdEncryptorForTest::PlaintextAndEncrypted id_pair;
    while (encrypted_id_queue.TryDequeue(id_pair).Successful()) {
      ASSERT_TRUE(found_ids.insert(id_pair.encrypted_id).second)
          << "Duplicate UUID was found";
    }
  }
  // Finish was called, but maybe there are more elements.
  RandomIdEncryptorForTest::PlaintextAndEncrypted id_pair;
  while (encrypted_id_queue.TryDequeue(id_pair).Successful()) {
    ASSERT_TRUE(found_ids.insert(id_pair.encrypted_id).second)
        << "Duplicate UUID was found";
  }

  WaitUntil([&finish_called]() { return finish_called.load(); });
  EXPECT_TRUE(encrypt_context.IsMarkedDone());

  EXPECT_EQ(found_ids.size(), GetNumIdsToEncrypt());
  EXPECT_TRUE(finish_called.load());
  if (producer_thread.joinable()) {
    producer_thread.join();
  }
}

TEST_P(RandomIdEncryptorTest, FetchesEncryptedIds) {
  atomic_bool finish_called{false};
  ConsumerStreamingContext<StreamEncryptedIdsRequest,
                           RandomIdEncryptorForTest::PlaintextAndEncrypted>
      stream_ids_context(GetNumIdsToEncrypt());
  absl::flat_hash_set<Uuid> expected_ids;
  stream_ids_context.process_callback = [&finish_called, &expected_ids](
                                            auto& context, bool is_finish) {
    auto id_pair = context.TryGetNextResponse();
    ASSERT_NE(id_pair, nullptr);
    EXPECT_TRUE(expected_ids.contains(id_pair->encrypted_id));
    if (is_finish) {
      EXPECT_SUCCESS(context.result);
      finish_called = true;
    }
  };

  auto& encrypted_id_queue = encryptor_.GetEncryptedIdsQueue();
  for (int i = 0; i < GetNumIdsToEncrypt(); i++) {
    auto uuid = Uuid::GenerateUuid();
    ASSERT_TRUE(expected_ids.insert(uuid).second);
    ASSERT_SUCCESS(encrypted_id_queue.TryEnqueue({string(), uuid}));
  }
  encryptor_.StreamEncryptedIds(stream_ids_context);

  // Trick the encryptor into being done.
  ProducerStreamingContext<string, EncryptResult> encrypt_context;
  encrypt_context.MarkDone();
  ASSERT_SUCCESS(encryptor_.Encrypt(encrypt_context));

  WaitUntil([&finish_called]() { return finish_called.load(); });
}

INSTANTIATE_TEST_SUITE_P(CardinalityTest, RandomIdEncryptorTest,
                         Values(10, 1000, 10000, 100000));

}  // namespace google::pair::publisher_list_generator::test
