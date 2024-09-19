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

#include <string>

#include "cc/public/core/interface/execution_result.h"

namespace google::pair::common {

/**
 * @brief Context used to put blobs in a streaming manner.
 *
 */
class PutBlobStreamContext {
 public:
  PutBlobStreamContext() = delete;

  /**
   * @brief Put the Blob Stream Context object
   *
   * @param bucket_name the bucket name to put data into
   * @param blob_path the blob path to put the object data
   * @param initial_data some initial data ready to uploaded
   */
  PutBlobStreamContext(
      const std::string& bucket_name, const std::string& blob_path,
      std::string initial_data,
      std::optional<google::cmrt::sdk::common::v1::CloudIdentityInfo>
          cloud_identity_info = std::nullopt)
      : bucket_name_(bucket_name),
        blob_path_(blob_path),
        initial_data_(std::move(initial_data)),
        cloud_identity_info_(std::move(cloud_identity_info)) {}

  const std::string& GetBucketName() const { return bucket_name_; }

  const std::string& GetBlobPath() const { return blob_path_; }

  std::string& GetInitialData() { return initial_data_; }

  std::optional<google::cmrt::sdk::common::v1::CloudIdentityInfo>&
  GetCloudIdentityInfo() {
    return cloud_identity_info_;
  }

 private:
  std::string bucket_name_;
  std::string blob_path_;
  std::string initial_data_;
  std::optional<google::cmrt::sdk::common::v1::CloudIdentityInfo>
      cloud_identity_info_;
};
}  // namespace google::pair::common