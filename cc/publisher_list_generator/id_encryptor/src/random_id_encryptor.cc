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

#include "random_id_encryptor.h"

#include <functional>
#include <string_view>
#include <utility>

#include "cc/public/core/interface/execution_result.h"

#include "error_codes.h"

using google::scp::core::AsyncExecutorInterface;
using google::scp::core::AsyncPriority;
using google::scp::core::ConsumerStreamingContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::ProducerStreamingContext;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::Uuid;
using std::bind;
using std::make_shared;
using std::move;
using std::scoped_lock;
using std::shared_ptr;
using std::string;

static constexpr size_t kEncryptedIdsQueueSize = 100000000;

namespace google::pair::publisher_list_generator {

RandomIdEncryptor::RandomIdEncryptor(
    shared_ptr<AsyncExecutorInterface> cpu_async_executor)
    : cpu_async_executor_(move(cpu_async_executor)),
      encrypted_ids_queue_(kEncryptedIdsQueueSize) {}

Uuid RandomIdEncryptor::GetUniqueUuid() {
  Uuid uuid;
  do {
    uuid = Uuid::GenerateUuid();
  } while (used_ids_.contains(uuid));
  used_ids_.insert(uuid);
  return uuid;
}

void RandomIdEncryptor::EncryptIdsInternal(
    google::scp::core::ProducerStreamingContext<std::string, EncryptResult>&
        encrypt_context) {
  // Get as many IDs as available.
  // TODO: This will potentially hog the AsyncExecutor but this is OK for the
  // current design.
  auto plaintext = encrypt_context.TryGetNextRequest();
  while (plaintext != nullptr) {
    auto encrypted_id = GetUniqueUuid();
    if (auto result = encrypted_ids_queue_.TryEnqueue(
            {move(*plaintext), move(encrypted_id)});
        !result.Successful()) {
      encrypt_context.result = result;
      encrypt_context.MarkDone();
      encrypt_context.Finish();
      done_encrypting_ = true;
    }
    plaintext = encrypt_context.TryGetNextRequest();
  }
  if (encrypt_context.IsMarkedDone()) {
    plaintext = encrypt_context.TryGetNextRequest();
    while (plaintext != nullptr) {
      auto encrypted_id = GetUniqueUuid();
      if (auto result = encrypted_ids_queue_.TryEnqueue(
              {move(*plaintext), move(encrypted_id)});
          !result.Successful()) {
        encrypt_context.result = result;
        encrypt_context.MarkDone();
        encrypt_context.Finish();
        done_encrypting_ = true;
      }
      plaintext = encrypt_context.TryGetNextRequest();
    }
    encrypt_context.result = SuccessExecutionResult();
    encrypt_context.response = make_shared<EncryptResult>();
    encrypt_context.Finish();
    done_encrypting_ = true;
  } else if (auto schedule_result = cpu_async_executor_->Schedule(
                 bind(&RandomIdEncryptor::EncryptIdsInternal, this,
                      encrypt_context),
                 AsyncPriority::Normal);
             !schedule_result.Successful()) {
    encrypt_context.result = schedule_result;
    encrypt_context.MarkDone();
    encrypt_context.Finish();
    done_encrypting_ = true;
  }
}

ExecutionResult RandomIdEncryptor::Encrypt(
    ProducerStreamingContext<string, EncryptResult>& encrypt_context) {
  if (!done_encrypting_.load() || !done_streaming_.load()) {
    return FailureExecutionResult(
        errors::ID_ENCRYPTOR_NOT_DONE_WITH_EXISTING_ENCRYPTION);
  }
  done_encrypting_ = false;
  done_streaming_ = false;
  return cpu_async_executor_->Schedule(
      bind(&RandomIdEncryptor::EncryptIdsInternal, this, encrypt_context),
      AsyncPriority::Normal);
}

void RandomIdEncryptor::StreamIdsInternal(
    ConsumerStreamingContext<StreamEncryptedIdsRequest, PlaintextAndEncrypted>&
        stream_ids_context) {
  // TODO: This will potentially hog the AsyncExecutor but this is OK for the
  // current design.
  PlaintextAndEncrypted id_pair;
  while (encrypted_ids_queue_.TryDequeue(id_pair).Successful()) {
    if (auto push_result = stream_ids_context.TryPushResponse(move(id_pair));
        !push_result.Successful()) {
      done_streaming_ = true;
      stream_ids_context.result = push_result;
      stream_ids_context.MarkDone();
      stream_ids_context.Finish();
      return;
    }
  }
  if (done_encrypting_) {
    while (encrypted_ids_queue_.TryDequeue(id_pair).Successful()) {
      if (auto push_result = stream_ids_context.TryPushResponse(move(id_pair));
          !push_result.Successful()) {
        done_streaming_ = true;
        stream_ids_context.result = push_result;
        stream_ids_context.MarkDone();
        stream_ids_context.Finish();
        return;
      }
    }
    done_streaming_ = true;
    stream_ids_context.result = SuccessExecutionResult();
    stream_ids_context.MarkDone();
    stream_ids_context.Finish();
    return;
  }
  if (auto schedule_result = cpu_async_executor_->Schedule(
          bind(&RandomIdEncryptor::StreamIdsInternal, this, stream_ids_context),
          AsyncPriority::Normal);
      !schedule_result.Successful()) {
    done_streaming_ = true;
    stream_ids_context.result = schedule_result;
    stream_ids_context.MarkDone();
    stream_ids_context.Finish();
  }
}

ExecutionResult RandomIdEncryptor::StreamEncryptedIds(
    ConsumerStreamingContext<StreamEncryptedIdsRequest, PlaintextAndEncrypted>&
        stream_ids_context) {
  return cpu_async_executor_->Schedule(
      bind(&RandomIdEncryptor::StreamIdsInternal, this, stream_ids_context),
      AsyncPriority::Normal);
}

}  // namespace google::pair::publisher_list_generator
