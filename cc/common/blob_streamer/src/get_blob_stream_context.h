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

#include <functional>
#include <string>
#include <string_view>

#include "cc/public/core/interface/execution_result.h"
#include "cc/public/cpio/proto/common/v1/cloud_identity_info.pb.h"

namespace google::pair::common {
/**
 * @brief This is the callback invoked when a data chunk is received from the
 * input stream.
 * @param chunk is the data chunk
 * @param is_done is set to true when the stream signals that it's done
 * @param result is the execution result of the streaming operation to be looked
 * at when is_done is set to true
 */
using GetBlobStreamChunkProcessorCallback =
    std::function<void(std::string_view chunk, bool is_done,
                       const scp::core::ExecutionResult& result)>;

/**
 * @brief Context used to get blobs in a streaming manner.
 *
 */
class GetBlobStreamContext {
 public:
  GetBlobStreamContext() = delete;

  /**
   * @brief Get the Blob Stream Context object
   *
   * @param bucket_name the bucket name to get data from
   * @param blob_path the blob path to read the object data
   * @param max_bytes_per_chunk how many bytes to stream per chunk
   * @param callback the callback to invoke with chunks of streamed data
   * @param cloud_identity_info If attestation is to be done, the
   * project ID and WIP provider to use.
   */
  GetBlobStreamContext(
      const std::string& bucket_name, const std::string& blob_path,
      const size_t max_bytes_per_chunk,
      const GetBlobStreamChunkProcessorCallback& callback,
      std::optional<google::cmrt::sdk::common::v1::CloudIdentityInfo>
          cloud_identity_info = std::nullopt)
      : bucket_name_(bucket_name),
        blob_path_(blob_path),
        max_bytes_per_chunk_(max_bytes_per_chunk),
        callback_(callback),
        cloud_identity_info_(std::move(cloud_identity_info)) {}

  const std::string& GetBucketName() const { return bucket_name_; }

  const std::string& GetBlobPath() const { return blob_path_; }

  size_t GetMaxBytesPerChunk() const { return max_bytes_per_chunk_; }

  const GetBlobStreamChunkProcessorCallback& GetCallback() const {
    return callback_;
  }

  std::optional<google::cmrt::sdk::common::v1::CloudIdentityInfo>&
  GetCloudIdentityInfo() {
    return cloud_identity_info_;
  }

 private:
  std::string bucket_name_;
  std::string blob_path_;
  size_t max_bytes_per_chunk_;
  GetBlobStreamChunkProcessorCallback callback_;
  std::optional<google::cmrt::sdk::common::v1::CloudIdentityInfo>
      cloud_identity_info_;
};
}  // namespace google::pair::common