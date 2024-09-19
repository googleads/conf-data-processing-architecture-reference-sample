// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <mutex>

#include "absl/container/flat_hash_set.h"
#include "cc/core/common/concurrent_queue/src/concurrent_queue.h"
#include "cc/core/common/uuid/src/uuid.h"
#include "cc/core/interface/async_executor_interface.h"
#include "cc/public/core/interface/execution_result.h"
#include "id_encryptor.h"

namespace google::pair::publisher_list_generator {

/**
 * @brief "Encrypts" PAIR IDs by simply generating a Uuid randomly.
 *
 */
class RandomIdEncryptor
    : public IdEncryptor<std::string, google::scp::core::common::Uuid> {
 public:
  explicit RandomIdEncryptor(
      std::shared_ptr<google::scp::core::AsyncExecutorInterface>
          cpu_async_executor);

  google::scp::core::ExecutionResult Encrypt(
      google::scp::core::ProducerStreamingContext<std::string, EncryptResult>&
          encrypt_context) override;

  google::scp::core::ExecutionResult StreamEncryptedIds(
      google::scp::core::ConsumerStreamingContext<StreamEncryptedIdsRequest,
                                                  PlaintextAndEncrypted>&
          stream_ids_context) override;

 protected:
  // TestOnly.
  google::scp::core::common::ConcurrentQueue<PlaintextAndEncrypted>&
  GetEncryptedIds() {
    return encrypted_ids_queue_;
  }

 private:
  // Call to be scheduled asynchronously to enable streaming from the provided
  // context.
  void EncryptIdsInternal(
      google::scp::core::ProducerStreamingContext<std::string, EncryptResult>&
          encrypt_context);

  // Call to be scheduled asynchronously to enable streaming to the provided
  // context.
  void StreamIdsInternal(google::scp::core::ConsumerStreamingContext<
                         StreamEncryptedIdsRequest, PlaintextAndEncrypted>&
                             stream_ids_context);

  // The name is not intuitive but this acquires a UUID that is guaranteed
  // unique to this object.
  google::scp::core::common::Uuid GetUniqueUuid();

  // Instance of an AsyncExecutor to do the asynchronous work on.
  std::shared_ptr<google::scp::core::AsyncExecutorInterface>
      cpu_async_executor_;
  // Whether all of the plaintext values have been encrypted.
  std::atomic_bool done_encrypting_{true}, done_streaming_{true};
  // A queue containing the pairs of values ready to be stream out.
  google::scp::core::common::ConcurrentQueue<PlaintextAndEncrypted>
      encrypted_ids_queue_;
  // A set of all of the already used UUIDs.
  absl::flat_hash_set<google::scp::core::common::Uuid> used_ids_;
};

}  // namespace google::pair::publisher_list_generator
