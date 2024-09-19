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

#include "match_worker.h"

#include "absl/strings/str_cat.h"
#include "cc/common/blob_streamer/src/get_blob_stream_context.h"
#include "cc/common/csv_parser/src/csv_stream_parser.h"
#include "cc/matcher/match_table/src/match_table_hash_map.h"
#include "cc/publisher_list_generator/proto/publisher_pair_list.pb.h"

#include "error_codes.h"

using google::cmrt::sdk::blob_storage_service::v1::GetBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobResponse;
using google::pair::common::BlobStreamerInterface;
using google::pair::common::CsvStreamParser;
using google::pair::common::CsvStreamParserConfig;
using google::pair::common::GetBlobStreamContext;
using google::pair::common::PutBlobCallback;
using google::pair::common::PutBlobStreamContext;
using google::pair::common::PutBlobStreamDoneMarker;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::cpio::BlobStorageClientInterface;
using std::atomic_bool;
using std::function;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;

namespace {

constexpr char kMatchWorker[] = "MatchWorker";
constexpr size_t kNumPublisherCsvColumns = 2;
constexpr size_t kNumAdvertiserCsvColumns = 1;
constexpr size_t kBytesPerResponse = 80 * 1024 * 1024;

// Forwards result to add_chunk_functor, indicating to the BlobStreamer that we
// should cancel the upload. This is only done if the upload has started i.e.
// add_chunk_functor is set.
void CancelUploadIfStarted(PutBlobCallback& add_chunk_functor,
                           ExecutionResult result) {
  if (add_chunk_functor) {
    add_chunk_functor(result);
  }
}

}  // namespace

namespace google::pair::matcher {

MatchWorker::MatchWorker(
    shared_ptr<BlobStorageClientInterface> blob_storage_client,
    unique_ptr<BlobStreamerInterface> blob_streamer)
    : blob_storage_client_(move(blob_storage_client)),
      blob_streamer_(move(blob_streamer)) {}

ExecutionResult MatchWorker::ParseBlobResponseIntoMatchTable(
    const string& blob_response) {
  // Parse the blob_response as a CSV where each row is a comma separated
  // key-value pairing.
  CsvStreamParser csv_parser{CsvStreamParserConfig(
      kNumPublisherCsvColumns, /* remove_whitespace */ true,
      /* delimiter */ kDefaultCsvRowDelimiter,
      /* line_break */ kDefaultCsvLineBreak,
      /* max_buffered_data_size */
      kMaxCsvStreamParserBufferedDataSizeBytes)};
  RETURN_IF_FAILURE(csv_parser.AddCsvChunk(blob_response));
  while (csv_parser.HasRow()) {
    ASSIGN_OR_RETURN(auto row, csv_parser.GetNextRow());
    ASSIGN_OR_RETURN(auto plaintext_id, row.GetColumn(0));
    ASSIGN_OR_RETURN(auto encrypted_id, row.GetColumn(1));
    RETURN_IF_FAILURE(match_table_->AddElement(plaintext_id, encrypted_id));
  }
  return SuccessExecutionResult();
}

ExecutionResult MatchWorker::GetExistingRows(
    const ExportMatchesRequest& request, CsvStreamParser& csv_parser,
    PutBlobCallback& add_chunk_functor) {
  while (csv_parser.HasRow()) {
    auto row_or = csv_parser.GetNextRow();
    if (!row_or.Successful()) {
      CancelUploadIfStarted(add_chunk_functor, row_or.result());
      return row_or.result();
    }
    auto plaintext_id_or = row_or->GetColumn(0);
    if (!plaintext_id_or.Successful()) {
      CancelUploadIfStarted(add_chunk_functor, plaintext_id_or.result());
      return plaintext_id_or.result();
    }
    // Mark the row as matched and get the corresponding encrypted ID for it
    // so we can add it to the upload.
    auto encrypted_id_or = match_table_->MarkMatched(*plaintext_id_or);
    // If it matched.
    if (encrypted_id_or.has_value()) {
      // If the upload stream hasn't been initiated yet, initiate it.
      if (!add_chunk_functor) {
        PutBlobStreamContext put_blob_context(
            request.output_bucket, request.matched_ids_name,
            absl::StrCat(encrypted_id_or.release(), "\n"),
            request.publisher_cloud_identity_info);
        ASSIGN_OR_RETURN(add_chunk_functor,
                         blob_streamer_->PutBlobStream(put_blob_context));
      } else {
        RETURN_IF_FAILURE(
            add_chunk_functor(absl::StrCat(encrypted_id_or.release(), "\n")));
      }
    }
  }
  return SuccessExecutionResult();
}

ExecutionResult MatchWorker::ExportMatches(
    const ExportMatchesRequest& request) {
  match_table_ = make_unique<MatchTableHashMap<string, string>>();
  // Acquire Pub mapping - blob_storage
  GetBlobRequest get_blob_request;
  get_blob_request.mutable_blob_metadata()->set_bucket_name(
      request.publisher_mapping_bucket);
  get_blob_request.mutable_blob_metadata()->set_blob_name(
      request.publisher_mapping_name);
  if (request.publisher_cloud_identity_info) {
    *get_blob_request.mutable_cloud_identity_info() =
        move(*request.publisher_cloud_identity_info);
  }
  ASSIGN_OR_RETURN(auto get_blob_response,
                   blob_storage_client_->GetBlobSync(get_blob_request));
  // Parse the mapping
  RETURN_IF_FAILURE(
      ParseBlobResponseIntoMatchTable(get_blob_response.blob().data()));
  // Stream Adv list
  CsvStreamParser csv_parser{CsvStreamParserConfig(
      kNumAdvertiserCsvColumns, /* remove_whitespace */ true,
      /* delimiter */ kDefaultCsvRowDelimiter,
      /* line_break */ kDefaultCsvLineBreak,
      /* max_buffered_data_size */
      kMaxCsvStreamParserBufferedDataSizeBytes)};
  atomic_bool all_advertiser_ids_received(false);
  ExecutionResult get_stream_result;
  RETURN_IF_FAILURE(blob_streamer_->GetBlobStream(GetBlobStreamContext(
      request.advertiser_list_bucket, request.advertiser_list_name,
      kBytesPerResponse,
      [&csv_parser, &get_stream_result, &all_advertiser_ids_received](
          auto chunk, bool is_done, const auto& result) {
        if (is_done) {
          // Do not overwrite get_stream_result if it has an error
          if (!get_stream_result.Successful()) {
            get_stream_result = result;
          }
          all_advertiser_ids_received = true;
        } else {
          // Forward the chunks to the CSV parser.
          auto add_chunk_result = csv_parser.AddCsvChunk(chunk);
          if (!add_chunk_result.Successful()) {
            get_stream_result = add_chunk_result;
          }
        }
      },
      request.advertiser_cloud_identity_info)));
  PutBlobCallback add_chunk_functor;
  // Loop through CSV parser and mark rows as matched - adding them to the
  // upload.
  while (!all_advertiser_ids_received) {
    RETURN_IF_FAILURE(GetExistingRows(request, csv_parser, add_chunk_functor));
  }
  if (!get_stream_result.Successful()) {
    CancelUploadIfStarted(add_chunk_functor, get_stream_result);
    return get_stream_result;
  }
  // TODO handle no IDs matched and upload an empty file. Creating an empty
  // file may not be supported by the BlobStorageClient API yet.
  RETURN_IF_FAILURE(GetExistingRows(request, csv_parser, add_chunk_functor));
  return add_chunk_functor(PutBlobStreamDoneMarker);
}

}  // namespace google::pair::matcher
