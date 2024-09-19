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

#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "cc/core/interface/async_executor_interface.h"
#include "cc/public/core/interface/execution_result.h"
#include "public/cpio/interface/blob_storage_client/blob_storage_client_interface.h"

#include "blob_streamer_interface.h"
#include "get_blob_stream_context.h"

namespace google::pair::common {
/**
 * @brief Blob streamer implementation, which leverages a CPIO blob storage
 * client to stream blob objects.
 *
 */
class BlobStreamer : public BlobStreamerInterface {
 public:
  /**
   * @brief Construct a new Blob Streamer object
   *
   * @param blob_storage_client the blob storage client instance
   * @param async_executor the async executor instance
   */
  BlobStreamer(
      std::shared_ptr<scp::core::AsyncExecutorInterface> async_executor,
      std::shared_ptr<scp::cpio::BlobStorageClientInterface>
          blob_storage_client);

  scp::core::ExecutionResult Init() noexcept override;

  scp::core::ExecutionResult Run() noexcept override;

  scp::core::ExecutionResult Stop() noexcept override;

  scp::core::ExecutionResult GetBlobStream(
      GetBlobStreamContext get_blob_context) noexcept override;

  scp::core::ExecutionResultOr<PutBlobCallback> PutBlobStream(
      PutBlobStreamContext put_blob_context) noexcept override;

 protected:
  std::shared_ptr<scp::core::AsyncExecutorInterface> async_executor_;

  std::shared_ptr<scp::cpio::BlobStorageClientInterface> blob_storage_client_;
  /**
   * @brief flag used to mark that the streamer has been stopped.
   *
   */
  std::atomic_bool stop_;
};
}  // namespace google::pair::common