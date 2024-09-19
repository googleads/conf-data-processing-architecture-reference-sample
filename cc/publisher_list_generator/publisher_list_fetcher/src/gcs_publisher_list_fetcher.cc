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

#include "gcs_publisher_list_fetcher.h"

#include <future>

#include "cc/common/csv_parser/src/csv_stream_parser.h"
#include "cc/core/common/global_logger/src/global_logger.h"
#include "cc/public/core/interface/execution_result.h"
#include "cc/public/cpio/proto/blob_storage_service/v1/blob_storage_service.pb.h"

#include "error_codes.h"

using google::cmrt::sdk::blob_storage_service::v1::GetBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::cpio::BlobStorageClientInterface;
using std::function;
using std::getline;
using std::ifstream;
using std::ios_base;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::strerror;
using std::string;
using std::vector;

namespace {

constexpr size_t kNumCsvColumns = 1;
constexpr char kGcsPublisherListFetcher[] = "GcsPublisherListFetcher";

}  // namespace

namespace google::pair::publisher_list_generator {

GcsPublisherListFetcher::GcsPublisherListFetcher(
    shared_ptr<BlobStorageClientInterface> blob_storage_client)
    : blob_storage_client_(move(blob_storage_client)),
      csv_parser_(
          make_unique<common::CsvStreamParser>(common::CsvStreamParserConfig(
              kNumCsvColumns, true, kDefaultCsvRowDelimiter,
              kDefaultCsvLineBreak,
              kMaxCsvStreamParserBufferedDataSizeBytes))) {}

ExecutionResultOr<FetchIdsResponse> GcsPublisherListFetcher::FetchPublisherIds(
    FetchIdsRequest request) {
  GetBlobRequest get_blob_request;
  get_blob_request.mutable_blob_metadata()->set_bucket_name(
      request.bucket_name);
  get_blob_request.mutable_blob_metadata()->set_blob_name(request.blob_name);
  if (request.cloud_identity_info) {
    *get_blob_request.mutable_cloud_identity_info() =
        *request.cloud_identity_info;
  }
  ASSIGN_OR_LOG_AND_RETURN(
      auto get_blob_response,
      blob_storage_client_->GetBlobSync(get_blob_request),
      kGcsPublisherListFetcher, kZeroUuid, "Failed getting ID blob %s/%s",
      request.bucket_name.c_str(), request.blob_name.c_str());

  RETURN_AND_LOG_IF_FAILURE(
      csv_parser_->AddCsvChunk(get_blob_response.blob().data()),
      kGcsPublisherListFetcher, kZeroUuid, "Failed adding CSV chunk");

  FetchIdsResponse response;
  auto row_or = csv_parser_->GetNextRow();
  while (row_or.Successful()) {
    ASSIGN_OR_LOG_AND_RETURN(auto id, row_or->GetColumn(0),
                             kGcsPublisherListFetcher, kZeroUuid,
                             "Failed getting column 0");
    response.ids.emplace_back(move(id));
    row_or = csv_parser_->GetNextRow();
  }

  return response;
}

}  // namespace google::pair::publisher_list_generator
