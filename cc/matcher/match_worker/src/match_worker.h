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

#include "cc/common/blob_streamer/src/blob_streamer.h"
#include "cc/common/csv_parser/src/csv_stream_parser.h"
#include "cc/matcher/match_table/src/match_table.h"
#include "cc/public/core/interface/execution_result.h"
#include "cc/public/cpio/interface/blob_storage_client/blob_storage_client_interface.h"

namespace google::pair::matcher {

struct ExportMatchesRequest {
  // The name of the bucket that the publisher mapping is in.
  std::string publisher_mapping_bucket;
  // The name of the publisher mapping in the bucket.
  std::string publisher_mapping_name;
  // The name of the bucket that the advertiser list is in.
  std::string advertiser_list_bucket;
  // The name of the advertiser list in the bucket.
  std::string advertiser_list_name;
  // The name of the bucket to output the mapping to.
  std::string output_bucket;
  // The name of the output list in the bucket.
  std::string matched_ids_name;
  // If attestation must be done, provide the publisher's project ID and WIP
  // provider here.
  std::optional<google::cmrt::sdk::common::v1::CloudIdentityInfo>
      publisher_cloud_identity_info;
  // If attestation must be done, provide the advertiser's project ID and WIP
  // provider here.
  std::optional<google::cmrt::sdk::common::v1::CloudIdentityInfo>
      advertiser_cloud_identity_info;
};

/**
 * @brief Class to use an existing Publisher PAIR mapping and an input
 * Advertiser IDs list to export the matched IDs into a Google-Owned bucket.
 *
 */
class MatchWorker {
 public:
  MatchWorker(std::shared_ptr<scp::cpio::BlobStorageClientInterface>
                  blob_storage_client,
              std::unique_ptr<common::BlobStreamerInterface> blob_streamer);

  /**
   * @brief Exports all of the matched IDs (encrypted IDs) between the publisher
   * and the advertiser
   *
   * @param request
   * @return scp::core::ExecutionResult
   */
  scp::core::ExecutionResult ExportMatches(const ExportMatchesRequest& request);

 private:
  scp::core::ExecutionResult ParseBlobResponseIntoMatchTable(
      const std::string& blob_response);

  scp::core::ExecutionResult GetExistingRows(
      const ExportMatchesRequest& request, common::CsvStreamParser& csv_parser,
      common::PutBlobCallback& add_chunk_functor);

  std::shared_ptr<scp::cpio::BlobStorageClientInterface> blob_storage_client_;
  std::unique_ptr<common::BlobStreamerInterface> blob_streamer_;
  std::unique_ptr<matcher::MatchTable<std::string, std::string>> match_table_;
};

}  // namespace google::pair::matcher
