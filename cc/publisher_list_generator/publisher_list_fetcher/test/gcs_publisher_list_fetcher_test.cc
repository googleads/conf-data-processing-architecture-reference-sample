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

#include "cc/publisher_list_generator/publisher_list_fetcher/src/gcs_publisher_list_fetcher.h"

#include <gtest/gtest.h>

#include "cc/common/attestation/src/attestation_info.h"
#include "cc/core/test/utils/proto_test_utils.h"
#include "cc/public/core/interface/execution_result.h"
#include "cc/public/core/test/interface/execution_result_matchers.h"
#include "cc/public/cpio/mock/blob_storage_client/mock_blob_storage_client.h"

using google::cmrt::sdk::blob_storage_service::v1::GetBlobResponse;
using google::pair::common::BuildGcpCloudIdentityInfo;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::test::EqualsProto;
using google::scp::core::test::ResultIs;
using google::scp::cpio::BlobStorageClientInterface;
using google::scp::cpio::MockBlobStorageClient;
using std::make_shared;
using std::move;
using std::shared_ptr;
using testing::Return;
using testing::UnorderedElementsAre;

namespace {
constexpr char kBucketName[] = "test_bucket";
constexpr char kBlobName[] = "test_blob";
}  // namespace

namespace google::pair::publisher_list_generator::test {

class GcsPublisherListFetcherTest : public testing::Test {
 protected:
  GcsPublisherListFetcherTest()
      : mock_blob_storage_client_(*new MockBlobStorageClient()),
        fetcher_(shared_ptr<BlobStorageClientInterface>(
            &mock_blob_storage_client_)) {}

  MockBlobStorageClient& mock_blob_storage_client_;
  GcsPublisherListFetcher fetcher_;
};

TEST_F(GcsPublisherListFetcherTest, ReturnsIds) {
  auto ids_string =
      "someone@gmail.com\n"
      "someone.else@yahoo.com\n"
      "yet.another.person@hotmail.com\n";
  EXPECT_CALL(mock_blob_storage_client_, GetBlobSync)
      .WillOnce([&ids_string](auto request) {
        EXPECT_EQ(request.blob_metadata().bucket_name(), kBucketName);
        EXPECT_EQ(request.blob_metadata().blob_name(), kBlobName);
        EXPECT_FALSE(request.has_cloud_identity_info())
            << request.cloud_identity_info().DebugString();

        GetBlobResponse response;
        response.mutable_blob()->set_data(ids_string);
        return response;
      });

  ASSERT_SUCCESS_AND_ASSIGN(
      auto fetch_ids_response,
      fetcher_.FetchPublisherIds({kBucketName, kBlobName}));

  EXPECT_THAT(
      fetch_ids_response.ids,
      UnorderedElementsAre("someone@gmail.com", "someone.else@yahoo.com",
                           "yet.another.person@hotmail.com"));
}

TEST_F(GcsPublisherListFetcherTest, PassesWipInfo) {
  auto ids_string =
      "someone@gmail.com\n"
      "someone.else@yahoo.com\n"
      "yet.another.person@hotmail.com\n";
  EXPECT_CALL(mock_blob_storage_client_, GetBlobSync)
      .WillOnce([&ids_string](auto request) {
        EXPECT_EQ(request.blob_metadata().bucket_name(), kBucketName);
        EXPECT_EQ(request.blob_metadata().blob_name(), kBlobName);
        EXPECT_THAT(
            request.cloud_identity_info(),
            EqualsProto(BuildGcpCloudIdentityInfo("project", "wip_provider")));

        GetBlobResponse response;
        response.mutable_blob()->set_data(ids_string);
        return response;
      });

  ASSERT_SUCCESS_AND_ASSIGN(
      auto fetch_ids_response,
      fetcher_.FetchPublisherIds(
          {kBucketName, kBlobName,
           BuildGcpCloudIdentityInfo("project", "wip_provider")}));

  EXPECT_THAT(
      fetch_ids_response.ids,
      UnorderedElementsAre("someone@gmail.com", "someone.else@yahoo.com",
                           "yet.another.person@hotmail.com"));
}

TEST_F(GcsPublisherListFetcherTest, FailsOnSyncFailure) {
  EXPECT_CALL(mock_blob_storage_client_, GetBlobSync)
      .WillOnce(Return(FailureExecutionResult(12345)));

  EXPECT_THAT(fetcher_.FetchPublisherIds({kBucketName, kBlobName}),
              ResultIs(FailureExecutionResult(12345)));
}

}  // namespace google::pair::publisher_list_generator::test
