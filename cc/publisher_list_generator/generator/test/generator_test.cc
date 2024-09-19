/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cc/publisher_list_generator/generator/src/generator.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

#include "absl/strings/str_cat.h"
#include "cc/common/attestation/src/attestation_info.h"
#include "cc/core/async_executor/src/async_executor.h"
#include "cc/core/test/utils/proto_test_utils.h"
#include "cc/public/core/interface/execution_result.h"
#include "cc/public/core/test/interface/execution_result_matchers.h"
#include "cc/public/cpio/mock/blob_storage_client/mock_blob_storage_client.h"
#include "cc/publisher_list_generator/id_encryptor/src/random_id_encryptor.h"
#include "cc/publisher_list_generator/proto/publisher_pair_list.pb.h"
#include "cc/publisher_list_generator/publisher_list_fetcher/mock/mock_publisher_list_fetcher.h"
#include "cc/publisher_list_generator/publisher_mapping_uploader/mock/mock_publisher_mapping_uploader.h"

using google::cmrt::sdk::blob_storage_service::v1::GetBlobResponse;
using google::pair::common::BuildGcpCloudIdentityInfo;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutor;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::Uuid;
using google::scp::core::test::EqualsProto;
using google::scp::core::test::ResultIs;
using google::scp::cpio::BlobStorageClientInterface;
using google::scp::cpio::MockBlobStorageClient;
using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;
using testing::Eq;
using testing::ExplainMatchResult;
using testing::FieldsAre;
using testing::Optional;
using testing::Pair;
using testing::Return;
using testing::UnorderedElementsAreArray;

namespace {

constexpr char kBucketName[] = "test_bucket";
constexpr char kListName[] = "test_list";
constexpr char kMetadataName[] = "test_metadata";
constexpr char kOutputBucketName[] = "output_bucket";
constexpr char kGeneratedListName[] = "generated";

}  // namespace

namespace google::pair::publisher_list_generator::test {

class GeneratorTest : public testing::Test {
 protected:
  GeneratorTest()
      : async_executor_(make_shared<AsyncExecutor>(10, 100)),
        mock_list_fetcher_(*new MockPublisherListFetcher()),
        mock_blob_storage_client_(*new MockBlobStorageClient()),
        mock_uploader_(*new MockPublisherMappingUploader()),
        generator_(unique_ptr<PublisherListFetcher>(&mock_list_fetcher_),
                   make_unique<RandomIdEncryptor>(async_executor_),
                   unique_ptr<PublisherMappingUploader>(&mock_uploader_),
                   shared_ptr<BlobStorageClientInterface>(
                       &mock_blob_storage_client_)) {
    EXPECT_SUCCESS(async_executor_->Init());
    EXPECT_SUCCESS(async_executor_->Run());
  }

  ~GeneratorTest() { EXPECT_SUCCESS(async_executor_->Stop()); }

  shared_ptr<AsyncExecutorInterface> async_executor_;
  MockPublisherListFetcher& mock_list_fetcher_;
  MockBlobStorageClient& mock_blob_storage_client_;
  MockPublisherMappingUploader& mock_uploader_;

  Generator<string, Uuid> generator_;
};

MATCHER_P(PublisherMappingHasIds, expected_ids, "") {
  const string& mapping = arg;
  vector<string> actual_ids;
  for (const auto& row : absl::StrSplit(mapping, "\n")) {
    if (row.empty()) continue;
    vector<string> cols = absl::StrSplit(row, ",");
    actual_ids.push_back(cols[0]);
  }
  return ExplainMatchResult(UnorderedElementsAreArray(expected_ids), actual_ids,
                            result_listener);
}

TEST_F(GeneratorTest, FetchesMapsAndUploads) {
  vector<string> ids({"id1", "id2", "id3"});
  EXPECT_CALL(
      mock_list_fetcher_,
      FetchPublisherIds(FieldsAre(kBucketName, kListName, Eq(std::nullopt))))
      .WillOnce(Return(FetchIdsResponse{ids}));
  EXPECT_CALL(mock_blob_storage_client_, GetBlobSync)
      .WillOnce([](auto request) {
        EXPECT_EQ(request.blob_metadata().bucket_name(), kBucketName);
        EXPECT_EQ(request.blob_metadata().blob_name(), kMetadataName);
        EXPECT_FALSE(request.has_cloud_identity_info())
            << request.cloud_identity_info().DebugString();

        GetBlobResponse response;
        response.mutable_blob()->set_data(kOutputBucketName);
        return response;
      });
  EXPECT_CALL(mock_uploader_,
              UploadIdMapping(
                  FieldsAre(kOutputBucketName, std::nullopt, kGeneratedListName,
                            PublisherMappingHasIds(ids), Eq(std::nullopt))))
      .WillOnce(Return(SuccessExecutionResult()));
  EXPECT_SUCCESS(generator_.GeneratePublisherList(
      {kBucketName, kListName, kMetadataName, kGeneratedListName}));
}

TEST_F(GeneratorTest, PassesWipProvider) {
  vector<string> ids({"id1", "id2", "id3"});
  EXPECT_CALL(mock_list_fetcher_,
              FetchPublisherIds(
                  FieldsAre(kBucketName, kListName,
                            Optional(EqualsProto(BuildGcpCloudIdentityInfo(
                                "project", "wip_provider"))))))
      .WillOnce(Return(FetchIdsResponse{ids}));
  EXPECT_CALL(mock_blob_storage_client_, GetBlobSync)
      .WillOnce([](auto request) {
        EXPECT_EQ(request.blob_metadata().bucket_name(), kBucketName);
        EXPECT_EQ(request.blob_metadata().blob_name(), kMetadataName);
        EXPECT_THAT(
            request.cloud_identity_info(),
            EqualsProto(BuildGcpCloudIdentityInfo("project", "wip_provider")));

        GetBlobResponse response;
        response.mutable_blob()->set_data(kOutputBucketName);
        return response;
      });
  EXPECT_CALL(
      mock_uploader_,
      UploadIdMapping(FieldsAre(kOutputBucketName, std::nullopt,
                                kGeneratedListName, PublisherMappingHasIds(ids),
                                Optional(EqualsProto(BuildGcpCloudIdentityInfo(
                                    "project", "wip_provider"))))))
      .WillOnce(Return(SuccessExecutionResult()));
  EXPECT_SUCCESS(generator_.GeneratePublisherList(
      {kBucketName, kListName, kMetadataName, kGeneratedListName,
       BuildGcpCloudIdentityInfo("project", "wip_provider")}));
}

TEST_F(GeneratorTest, FailsIfFetchingFails) {
  vector<string> ids({"id1", "id2", "id3"});
  EXPECT_CALL(
      mock_list_fetcher_,
      FetchPublisherIds(FieldsAre(kBucketName, kListName, Eq(std::nullopt))))
      .WillOnce(Return(FailureExecutionResult(12345)));
  EXPECT_CALL(mock_blob_storage_client_, GetBlobSync).Times(0);
  EXPECT_CALL(mock_uploader_, UploadIdMapping).Times(0);
  EXPECT_THAT(generator_.GeneratePublisherList(
                  {kBucketName, kListName, kMetadataName, kGeneratedListName}),
              ResultIs(FailureExecutionResult(12345)));
}

TEST_F(GeneratorTest, FailsIfMetadataFetchingFails) {
  vector<string> ids({"id1", "id2", "id3"});
  EXPECT_CALL(
      mock_list_fetcher_,
      FetchPublisherIds(FieldsAre(kBucketName, kListName, Eq(std::nullopt))))
      .WillOnce(Return(FetchIdsResponse{ids}));
  EXPECT_CALL(mock_blob_storage_client_, GetBlobSync)
      .WillOnce([](auto request) {
        EXPECT_EQ(request.blob_metadata().bucket_name(), kBucketName);
        EXPECT_EQ(request.blob_metadata().blob_name(), kMetadataName);
        return FailureExecutionResult(12345);
      });
  EXPECT_CALL(mock_uploader_, UploadIdMapping).Times(0);
  EXPECT_THAT(generator_.GeneratePublisherList(
                  {kBucketName, kListName, kMetadataName, kGeneratedListName}),
              ResultIs(FailureExecutionResult(12345)));
}

TEST_F(GeneratorTest, FailsIfUploadingFails) {
  vector<string> ids({"id1", "id2", "id3"});
  EXPECT_CALL(
      mock_list_fetcher_,
      FetchPublisherIds(FieldsAre(kBucketName, kListName, Eq(std::nullopt))))
      .WillOnce(Return(FetchIdsResponse{ids}));
  EXPECT_CALL(mock_blob_storage_client_, GetBlobSync).WillOnce([](auto) {
    GetBlobResponse response;
    response.mutable_blob()->set_data(kOutputBucketName);
    return response;
  });
  EXPECT_CALL(mock_uploader_,
              UploadIdMapping(
                  FieldsAre(kOutputBucketName, std::nullopt, kGeneratedListName,
                            PublisherMappingHasIds(ids), Eq(std::nullopt))))
      .WillOnce(Return(FailureExecutionResult(12345)));
  EXPECT_THAT(generator_.GeneratePublisherList(
                  {kBucketName, kListName, kMetadataName, kGeneratedListName}),
              ResultIs(FailureExecutionResult(12345)));
}

}  // namespace google::pair::publisher_list_generator::test
