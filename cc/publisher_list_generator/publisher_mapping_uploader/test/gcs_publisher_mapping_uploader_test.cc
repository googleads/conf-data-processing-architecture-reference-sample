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

#include "cc/publisher_list_generator/publisher_mapping_uploader/src/gcs_publisher_mapping_uploader.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

#include "absl/strings/str_cat.h"
#include "cc/common/attestation/src/attestation_info.h"
#include "cc/core/test/utils/proto_test_utils.h"
#include "cc/public/core/interface/execution_result.h"
#include "cc/public/core/test/interface/execution_result_matchers.h"
#include "cc/public/cpio/mock/blob_storage_client/mock_blob_storage_client.h"
#include "cc/publisher_list_generator/proto/publisher_pair_list.pb.h"

using google::cmrt::sdk::blob_storage_service::v1::PutBlobResponse;
using google::pair::common::BuildGcpCloudIdentityInfo;
using google::scp::core::AsyncContext;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::test::EqualsProto;
using google::scp::core::test::ResultIs;
using google::scp::cpio::BlobStorageClientInterface;
using google::scp::cpio::MockBlobStorageClient;
using std::make_shared;
using std::shared_ptr;
using std::string;
using testing::Return;

namespace {
constexpr char kBucketName[] = "test_bucket";
constexpr char kMappingName[] = "test_mapping";
}  // namespace

namespace google::pair::publisher_list_generator::test {

class GcsPublisherMappingUploaderTest : public testing::Test {
 protected:
  GcsPublisherMappingUploaderTest()
      : mock_blob_storage_client_(*new MockBlobStorageClient()),
        uploader_(shared_ptr<BlobStorageClientInterface>(
            &mock_blob_storage_client_)) {}

  MockBlobStorageClient& mock_blob_storage_client_;
  GcsPublisherMappingUploader uploader_;
};

TEST_F(GcsPublisherMappingUploaderTest, CallsPutBlob) {
  string mapping = "key1,val1\nkey2,val2\n";
  EXPECT_CALL(mock_blob_storage_client_, PutBlobSync)
      .WillOnce([&mapping](auto request) {
        EXPECT_EQ(request.blob().metadata().bucket_name(), kBucketName);
        EXPECT_EQ(request.blob().metadata().blob_name(), kMappingName);
        EXPECT_FALSE(request.has_cloud_identity_info())
            << request.cloud_identity_info().DebugString();

        EXPECT_EQ(mapping, request.blob().data());

        return PutBlobResponse();
      });
  EXPECT_SUCCESS(uploader_.UploadIdMapping(
      {kBucketName, std::nullopt, kMappingName, mapping}));
}

TEST_F(GcsPublisherMappingUploaderTest, PassesWipProvider) {
  string mapping = "key1,val1\nkey2,val2\n";
  EXPECT_CALL(mock_blob_storage_client_, PutBlobSync)
      .WillOnce([&mapping](auto request) {
        EXPECT_EQ(request.blob().metadata().bucket_name(), kBucketName);
        EXPECT_EQ(request.blob().metadata().blob_name(), kMappingName);
        EXPECT_THAT(
            request.cloud_identity_info(),
            EqualsProto(BuildGcpCloudIdentityInfo("project", "wip_provider")));

        EXPECT_EQ(mapping, request.blob().data());

        return PutBlobResponse();
      });
  EXPECT_SUCCESS(uploader_.UploadIdMapping(
      {kBucketName, std::nullopt, kMappingName, mapping,
       BuildGcpCloudIdentityInfo("project", "wip_provider")}));
}

TEST_F(GcsPublisherMappingUploaderTest, CallsPutBlobWithPrefix) {
  string mapping = "key1,val1\nkey2,val2\n";
  EXPECT_CALL(mock_blob_storage_client_, PutBlobSync)
      .WillOnce([&mapping](auto request) {
        EXPECT_EQ(request.blob().metadata().bucket_name(), kBucketName);
        EXPECT_EQ(request.blob().metadata().blob_name(),
                  absl::StrCat("prefix/", kMappingName));

        EXPECT_EQ(mapping, request.blob().data());

        return PutBlobResponse();
      });
  EXPECT_SUCCESS(uploader_.UploadIdMapping(
      {kBucketName, "prefix", kMappingName, mapping}));
}

TEST_F(GcsPublisherMappingUploaderTest, PropagatesErrorFromPutBlobSync) {
  EXPECT_CALL(mock_blob_storage_client_, PutBlobSync)
      .WillOnce(Return(FailureExecutionResult(12345)));
  EXPECT_THAT(uploader_.UploadIdMapping({kBucketName, std::nullopt,
                                         kMappingName, string()}),
              ResultIs(FailureExecutionResult(12345)));
}

}  // namespace google::pair::publisher_list_generator::test
