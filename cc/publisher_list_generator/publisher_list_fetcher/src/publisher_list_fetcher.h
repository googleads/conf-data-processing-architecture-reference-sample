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
#include "cc/public/cpio/proto/common/v1/cloud_identity_info.pb.h"

namespace google::pair::publisher_list_generator {

struct FetchIdsRequest {
  // The name of the bucket containing the IDs to fetch.
  std::string bucket_name;
  // The name of the object containing the IDs to fetch.
  std::string blob_name;
  // The attested credentials with which to use to download the blob.
  std::optional<google::cmrt::sdk::common::v1::CloudIdentityInfo>
      cloud_identity_info;
};

struct FetchIdsResponse {
  // The IDs that have been fetched.
  std::vector<std::string> ids;
};

/**
 * @brief Interface for fetching the publisher list from storage.
 *
 */
class PublisherListFetcher {
 public:
  /**
   * @brief Fetches the publisher IDs from storage.
   *
   * @param request
   * @return google::scp::core::ExecutionResultOr<FetchIdsResponse>
   */
  virtual google::scp::core::ExecutionResultOr<FetchIdsResponse>
  FetchPublisherIds(FetchIdsRequest request) = 0;

  virtual ~PublisherListFetcher() = default;
};

}  // namespace google::pair::publisher_list_generator
