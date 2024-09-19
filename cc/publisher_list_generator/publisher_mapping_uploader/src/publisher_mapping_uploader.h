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

#include <optional>
#include <string>
#include <vector>

#include "cc/public/core/interface/execution_result.h"
#include "cc/publisher_list_generator/proto/publisher_pair_list.pb.h"

namespace google::pair::publisher_list_generator {

/**
 * @brief A request to upload a PublisherPairMapping.
 *
 */
struct UploadMappingRequest {
  // Name of the bucket to upload to.
  std::string bucket_name;
  // A prefix to add to the upload_name - if provided, a trailing '/' will be
  // automatically added.
  std::optional<std::string> prefix;
  // The name of the mapping.
  std::string upload_name;
  // The data to upload.
  std::string mapping;
  // If attestation must be done, provide the project ID and WIP provider here.
  std::optional<google::cmrt::sdk::common::v1::CloudIdentityInfo>
      cloud_identity_info;
};

/**
 * @brief Interface for uploading the publisher mapping to storage.
 *
 */
class PublisherMappingUploader {
 public:
  /**
   * @brief Uploads the mapping into storage.
   *
   * @param request The details of the upload.
   * @return google::scp::core::ExecutionResult
   */
  virtual google::scp::core::ExecutionResult UploadIdMapping(
      UploadMappingRequest request) = 0;

  virtual ~PublisherMappingUploader() = default;
};

}  // namespace google::pair::publisher_list_generator
