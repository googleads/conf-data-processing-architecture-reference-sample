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

#include "gcs_publisher_mapping_uploader.h"

#include <optional>
#include <string>

#include "absl/strings/str_cat.h"
#include "cc/public/core/interface/execution_result.h"
#include "cc/public/cpio/proto/blob_storage_service/v1/blob_storage_service.pb.h"
#include "cc/publisher_list_generator/proto/publisher_pair_list.pb.h"

using google::cmrt::sdk::blob_storage_service::v1::PutBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::cpio::BlobStorageClientInterface;
using std::function;
using std::move;
using std::optional;
using std::shared_ptr;
using std::string;
using std::string_view;

namespace {
constexpr char kGcsPublisherMappingUploader[] = "GcsPublisherMappingUploader";

string GetMappingName(const optional<string>& prefix, string_view upload_name) {
  if (prefix.has_value()) {
    return absl::StrCat(*prefix, "/", upload_name);
  }
  return string(upload_name);
}

}  // namespace

namespace google::pair::publisher_list_generator {

GcsPublisherMappingUploader::GcsPublisherMappingUploader(
    shared_ptr<BlobStorageClientInterface> blob_storage_client)
    : blob_storage_client_(move(blob_storage_client)) {}

ExecutionResult GcsPublisherMappingUploader::UploadIdMapping(
    UploadMappingRequest request) {
  PutBlobRequest put_blob_request;
  put_blob_request.mutable_blob()->mutable_metadata()->set_bucket_name(
      request.bucket_name);
  put_blob_request.mutable_blob()->mutable_metadata()->set_blob_name(
      GetMappingName(request.prefix, request.upload_name));
  put_blob_request.mutable_blob()->set_data(move(request.mapping));
  if (request.cloud_identity_info) {
    *put_blob_request.mutable_cloud_identity_info() =
        move(*request.cloud_identity_info);
  }
  return blob_storage_client_->PutBlobSync(put_blob_request).result();
}

}  // namespace google::pair::publisher_list_generator
