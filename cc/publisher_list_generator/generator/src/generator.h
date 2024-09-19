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

#include "absl/strings/str_cat.h"
#include "cc/core/interface/streaming_context.h"
#include "cc/public/core/interface/execution_result.h"
#include "cc/public/cpio/interface/blob_storage_client/blob_storage_client_interface.h"
#include "cc/public/cpio/proto/common/v1/cloud_identity_info.pb.h"
#include "cc/publisher_list_generator/id_encryptor/src/id_encryptor.h"
#include "cc/publisher_list_generator/proto/publisher_pair_list.pb.h"
#include "cc/publisher_list_generator/publisher_list_fetcher/src/publisher_list_fetcher.h"
#include "cc/publisher_list_generator/publisher_mapping_uploader/src/publisher_mapping_uploader.h"

namespace google::pair::publisher_list_generator {

struct GeneratePublisherListRequest {
  // The name of the bucket to read the list from.
  std::string bucket_name;
  // The (fully qualified) name of the object in bucket_name to read the list
  // from.
  std::string blob_name;
  // The name of the metadata object containing details
  // about the list to generate.
  std::string metadata_name;
  // The name of the list to use for the generated list.
  std::string generated_list_name;
  // The project ID and WIP provider to do attestation with.
  std::optional<google::cmrt::sdk::common::v1::CloudIdentityInfo>
      cloud_identity_info;
};

/**
 * @brief Uploads a Publisher ID Mapping to GCS.
 *
 */
template <typename PlaintextValue, typename EncryptedValue>
class Generator {
  using PlaintextAndEncrypted =
      typename IdEncryptor<PlaintextValue,
                           EncryptedValue>::PlaintextAndEncrypted;

 public:
  Generator(
      std::unique_ptr<PublisherListFetcher> list_fetcher,
      std::unique_ptr<IdEncryptor<PlaintextValue, EncryptedValue>> id_encryptor,
      std::unique_ptr<PublisherMappingUploader> mapping_uploader,
      std::shared_ptr<scp::cpio::BlobStorageClientInterface>
          blob_storage_client)
      : list_fetcher_(move(list_fetcher)),
        id_encryptor_(move(id_encryptor)),
        mapping_uploader_(move(mapping_uploader)),
        blob_storage_client_(blob_storage_client) {}

  ~Generator() {
    if (pushing_thread_.joinable()) {
      pushing_thread_.join();
    }
  }

  /**
   * @brief Reads the Publisher's plaintext ID list from blob_name in
   * bucket_name, generates encrypted IDs and then uploads the mapping to output
   * bucket specified by the file in bucket_name/metadata_name.
   *
   * @param request
   * @return ExecutionResult
   */
  scp::core::ExecutionResult GeneratePublisherList(
      GeneratePublisherListRequest request) {
    // Fetch the list
    ASSIGN_OR_LOG_AND_RETURN(auto fetch_response,
                             list_fetcher_->FetchPublisherIds(
                                 {request.bucket_name, request.blob_name,
                                  request.cloud_identity_info}),
                             kGenerator, scp::core::common::kZeroUuid,
                             "Failed fetching Publisher IDs");

    // Fetch the bucket name to upload to
    ASSIGN_OR_LOG_AND_RETURN(
        std::string output_bucket,
        GetOutputBucketName(request.bucket_name, request.metadata_name,
                            request.cloud_identity_info),
        kGenerator, scp::core::common::kZeroUuid,
        "Failed getting output bucket name");

    // Encrypt the IDs

    std::atomic_bool encryption_done(false);
    scp::core::ExecutionResult encryption_result, pushing_result;
    RETURN_AND_LOG_IF_FAILURE(
        BeginEncryption(fetch_response, encryption_done, encryption_result,
                        pushing_result),
        kGenerator, scp::core::common::kZeroUuid,
        "Failed beginning encryption");

    ASSIGN_OR_LOG_AND_RETURN(
        auto encrypted_pairs, StreamIds(fetch_response.ids.size()), kGenerator,
        scp::core::common::kZeroUuid, "Failed streaming IDs");

    while (!encryption_done.load());
    if (pushing_thread_.joinable()) {
      pushing_thread_.join();
    }

    RETURN_AND_LOG_IF_FAILURE(pushing_result, kGenerator,
                              scp::core::common::kZeroUuid,
                              "Pushing async failed");
    RETURN_AND_LOG_IF_FAILURE(encryption_result, kGenerator,
                              scp::core::common::kZeroUuid,
                              "Encryption async failed");

    // Upload the mapping

    std::string mapping;
    for (const auto& pair : encrypted_pairs) {
      absl::StrAppend(&mapping, pair.plaintext, ",",
                      scp::core::common::ToString(pair.encrypted_id), "\n");
    }
    return mapping_uploader_->UploadIdMapping(
        {output_bucket, std::nullopt, request.generated_list_name, std::move(mapping),
         request.cloud_identity_info});
  }

 private:
  /**
   * @brief Gets the name of the output bucket to upload to.
   *
   * @param bucket_name Name of the bucket to find the metadata file in.
   * @param metadata_name Name of the metadata file in bucket_name.
   * @return scp::core::ExecutionResultOr<std::string> The name of the output
   * bucket or an error.
   */
  scp::core::ExecutionResultOr<std::string> GetOutputBucketName(
      std::string_view bucket_name, std::string_view metadata_name,
      std::optional<google::cmrt::sdk::common::v1::CloudIdentityInfo>
          cloud_identity_info) {
    cmrt::sdk::blob_storage_service::v1::GetBlobRequest request;
    request.mutable_blob_metadata()->set_bucket_name(std::string(bucket_name));
    request.mutable_blob_metadata()->set_blob_name(std::string(metadata_name));
    if (cloud_identity_info) {
      *request.mutable_cloud_identity_info() = std::move(*cloud_identity_info);
    }

    ASSIGN_OR_RETURN(auto response, blob_storage_client_->GetBlobSync(request));
    return std::move(*response.mutable_blob()->mutable_data());
  }

  /**
   * @brief Begins encryption of the IDs
   *
   * @param fetch_response The response containing the IDs to encrypt.
   * @param encryption_done Will be flipped once encryption has completed.
   * @param encryption_result Will have the result of encryption once
   * encryption_done is flipped.
   * @param pushing_result Indicates the result of pushing the IDs onto the
   * encryptor. Will be valid once pushing_thread_ is joined.
   * @return scp::core::ExecutionResult
   */
  scp::core::ExecutionResult BeginEncryption(
      const FetchIdsResponse& fetch_response, std::atomic_bool& encryption_done,
      scp::core::ExecutionResult& encryption_result,
      scp::core::ExecutionResult& pushing_result) {
    scp::core::ProducerStreamingContext<PlaintextValue, EncryptResult>
        encrypt_context(fetch_response.ids.size());

    encrypt_context.callback = [&encryption_done,
                                &encryption_result](auto& context) {
      encryption_result = context.result;
      encryption_done = true;
    };
    RETURN_AND_LOG_IF_FAILURE_CONTEXT(id_encryptor_->Encrypt(encrypt_context),
                                      kGenerator, encrypt_context,
                                      "Failed encrypting IDs");
    // Push the plain IDs asynchronously.
    pushing_thread_ = std::thread([&fetch_response, encrypt_context,
                                   &pushing_result]() mutable {
      for (auto& id : fetch_response.ids) {
        if (auto push_result = encrypt_context.TryPushRequest(std::move(id));
            !push_result.Successful()) {
          SCP_ERROR_CONTEXT(kGenerator, encrypt_context, push_result,
                            "Failed pushing IDs");
          pushing_result = push_result;
          encrypt_context.MarkDone();
          return;
        }
      }
      pushing_result = scp::core::SuccessExecutionResult();
      encrypt_context.MarkDone();
    });
    return scp::core::SuccessExecutionResult();
  }

  /**
   * @brief Acquires the plaintext IDs and their encrypted counterparts.
   *
   * @param num_ids The number of IDs that were encrypted.
   * @return scp::core::ExecutionResultOr<std::vector<PlaintextAndEncrypted>>
   */
  scp::core::ExecutionResultOr<std::vector<PlaintextAndEncrypted>> StreamIds(
      size_t num_ids) {
    scp::core::ExecutionResult streaming_result;
    std::atomic_bool streaming_done{false};
    scp::core::ConsumerStreamingContext<StreamEncryptedIdsRequest,
                                        PlaintextAndEncrypted>
        streaming_context(num_ids);
    streaming_context.process_callback = [&streaming_result, &streaming_done](
                                             auto& context, bool is_finish) {
      if (is_finish) {
        streaming_result = context.result;
        streaming_done = true;
      }
    };
    RETURN_AND_LOG_IF_FAILURE_CONTEXT(
        id_encryptor_->StreamEncryptedIds(streaming_context), kGenerator,
        streaming_context, "Failed streaming IDs");
    return RetrieveAllIds(num_ids, streaming_context, streaming_done,
                          streaming_result);
  }

  /**
   * @brief Acquires the plaintext and encrypted IDs out of streaming_context
   * until the context is done.
   *
   * @tparam Context
   * @param num_ids
   * @param streaming_context
   * @param streaming_result
   * @return scp::core::ExecutionResultOr<std::vector<PlaintextAndEncrypted>>
   */
  template <typename Context>
  scp::core::ExecutionResultOr<std::vector<PlaintextAndEncrypted>>
  RetrieveAllIds(size_t num_ids, Context& streaming_context,
                 const std::atomic_bool& streaming_done,
                 const scp::core::ExecutionResult& streaming_result) {
    std::vector<PlaintextAndEncrypted> encrypted_pairs;
    encrypted_pairs.reserve(num_ids);
    auto encrypted_id = streaming_context.TryGetNextResponse();
    while (encrypted_id != nullptr || !streaming_done.load()) {
      if (encrypted_id) {
        encrypted_pairs.emplace_back(std::move(*encrypted_id));
      }
      encrypted_id = streaming_context.TryGetNextResponse();
    }
    // In case more responses were enqueued in an edge case.
    encrypted_id = streaming_context.TryGetNextResponse();
    while (encrypted_id != nullptr) {
      encrypted_pairs.emplace_back(std::move(*encrypted_id));
      encrypted_id = streaming_context.TryGetNextResponse();
    }
    // streaming_result is ready now that streaming_context is done.
    RETURN_IF_FAILURE(streaming_result);
    return encrypted_pairs;
  }

  static constexpr char kGenerator[] = "PublisherListGenerator";

  std::unique_ptr<PublisherListFetcher> list_fetcher_;
  std::unique_ptr<IdEncryptor<PlaintextValue, EncryptedValue>> id_encryptor_;
  std::unique_ptr<PublisherMappingUploader> mapping_uploader_;
  std::shared_ptr<scp::cpio::BlobStorageClientInterface> blob_storage_client_;

  std::thread pushing_thread_;
};

}  // namespace google::pair::publisher_list_generator
