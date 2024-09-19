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

#include "cc/core/interface/streaming_context.h"
#include "cc/public/core/interface/execution_result.h"

namespace google::pair::publisher_list_generator {

struct EncryptResult {};
struct StreamEncryptedIdsRequest {};

/**
 * @brief Interface for encrypting PAIR IDs.
 *
 * @tparam PlaintextValue The type of the plaintext value.
 * @tparam EncryptedValue The type of the encrypted value.
 */
template <typename PlaintextValue, typename EncryptedValue>
class IdEncryptor {
 public:
  /**
   * @brief Container for the mapping of a PlaintextValue to its encrypted
   * counterpart.
   *
   */
  struct PlaintextAndEncrypted {
    PlaintextValue plaintext;
    EncryptedValue encrypted_id;
  };

  /**
   * @brief Opens the channel to begin encrypting the plaintext.
   *
   * @param encrypt_context The context to stream plaintext values on.
   * @return google::scp::core::ExecutionResult Whether the channel was opened
   * successfully.
   */
  virtual google::scp::core::ExecutionResult Encrypt(
      google::scp::core::ProducerStreamingContext<
          PlaintextValue, EncryptResult>& encrypt_context) = 0;

  /**
   * @brief Opens the channel to stream the encrypted IDs out.
   *
   * @param stream_ids_request The context to extract the encrypted IDs on.
   * @return google::scp::core::ExecutionResult Whether the channel was opened
   * successfully.
   */
  virtual google::scp::core::ExecutionResult StreamEncryptedIds(
      google::scp::core::ConsumerStreamingContext<StreamEncryptedIdsRequest,
                                                  PlaintextAndEncrypted>&
          stream_ids_request) = 0;

  virtual ~IdEncryptor() = default;
};

}  // namespace google::pair::publisher_list_generator
