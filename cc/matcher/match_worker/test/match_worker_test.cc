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

#include "cc/matcher/match_worker/src/match_worker.h"

#include <gtest/gtest.h>

#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "cc/common/attestation/src/attestation_info.h"
#include "cc/common/blob_streamer/mock/mock_blob_streamer.h"
#include "cc/matcher/match_table/src/error_codes.h"
#include "cc/public/core/interface/execution_result.h"
#include "cc/public/core/test/interface/execution_result_matchers.h"
#include "cc/public/cpio/mock/blob_storage_client/mock_blob_storage_client.h"
#include "cc/publisher_list_generator/proto/publisher_pair_list.pb.h"
#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/proto_test_utils.h"

using google::cmrt::sdk::blob_storage_service::v1::GetBlobResponse;
using google::pair::common::BlobStreamerInterface;
using google::pair::common::BuildGcpCloudIdentityInfo;
using google::pair::common::GetBlobStreamChunkProcessorCallback;
using google::pair::common::GetBlobStreamContext;
using google::pair::common::MockBlobStreamer;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::test::EqualsProto;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::MockBlobStorageClient;
using std::atomic_bool;
using std::atomic_int;
using std::initializer_list;
using std::make_shared;
using std::pair;
using std::shared_ptr;
using std::string;
using std::thread;
using std::unique_ptr;
using std::vector;
using std::chrono::milliseconds;
using std::this_thread::sleep_for;
using testing::Optional;
using testing::Pair;
using testing::Return;
using testing::UnorderedElementsAre;

namespace {
constexpr char kPublisherBucketName[] = "pub_bucket";
constexpr char kPublisherMapping[] = "pub_mapping";
constexpr char kAdvertiserBucketName[] = "adv_bucket";
constexpr char kAdvertiserList[] = "adv_list";
constexpr char kOutputBucketName[] = "output_bucket";
constexpr char kOutputList[] = "output_list";

constexpr char kEmail1[] = "key1", kEmail2[] = "key2", kEmail3[] = "key3";
constexpr char kEncrypted1[] = "val1", kEncrypted2[] = "val2",
               kEncrypted3[] = "val3";
}  // namespace

namespace google::pair::matcher::test {

class MatchWorkerTest : public testing::Test {
 protected:
  MatchWorkerTest()
      : blob_storage_client_(make_shared<MockBlobStorageClient>()),
        blob_streamer_(*new MockBlobStreamer()),
        matcher_(blob_storage_client_,
                 unique_ptr<BlobStreamerInterface>(&blob_streamer_)) {
    absl::StrAppend(&mapping_, kEmail1, ",", kEncrypted1, "\n");
    absl::StrAppend(&mapping_, kEmail2, ",", kEncrypted2, "\n");
    absl::StrAppend(&mapping_, kEmail3, ",", kEncrypted3, "\n");
  }

  shared_ptr<MockBlobStorageClient> blob_storage_client_;
  MockBlobStreamer& blob_streamer_;
  MatchWorker matcher_;
  string mapping_;
};

void CallCallbackWithEmails(const GetBlobStreamChunkProcessorCallback& callback,
                            initializer_list<string> l) {
  for (const auto& email : l) {
    callback(absl::StrCat(email, "\n"), false, SuccessExecutionResult());
  }
  callback("", true, SuccessExecutionResult());
}

// Splits ids_string on newlines '\n' - removes trailing empty string if
// present.
vector<string> IdsStringToVector(string& ids_string) {
  vector<string> v = absl::StrSplit(ids_string, '\n');
  if (v.back().empty()) v.pop_back();
  return v;
}

TEST_F(MatchWorkerTest, ExportWorks) {
  EXPECT_CALL(*blob_storage_client_, GetBlobSync)
      .WillOnce([this](auto request) {
        EXPECT_EQ(request.blob_metadata().bucket_name(), kPublisherBucketName);
        EXPECT_EQ(request.blob_metadata().blob_name(), kPublisherMapping);
        EXPECT_FALSE(request.has_cloud_identity_info())
            << request.cloud_identity_info().DebugString();

        GetBlobResponse response;
        response.mutable_blob()->set_data(mapping_);
        return response;
      });

  EXPECT_CALL(blob_streamer_, GetBlobStream).WillOnce([this](auto context) {
    EXPECT_EQ(context.GetBucketName(), kAdvertiserBucketName);
    EXPECT_EQ(context.GetBlobPath(), kAdvertiserList);
    EXPECT_FALSE(context.GetCloudIdentityInfo());

    CallCallbackWithEmails(context.GetCallback(), {kEmail1, kEmail3});
    return SuccessExecutionResult();
  });
  string matched_encrypted_ids_string;
  EXPECT_CALL(blob_streamer_, PutBlobStream)
      .WillOnce([this, &matched_encrypted_ids_string](auto context) {
        EXPECT_EQ(context.GetBucketName(), kOutputBucketName);
        EXPECT_EQ(context.GetBlobPath(), kOutputList);
        EXPECT_EQ(context.GetInitialData(), absl::StrCat(kEncrypted1, "\n"));
        EXPECT_FALSE(context.GetCloudIdentityInfo());
        matched_encrypted_ids_string += context.GetInitialData();

        // Expect all calls to be successful - record the chunks that come in.
        return
            [&matched_encrypted_ids_string](auto chunk_or) -> ExecutionResult {
              if (!chunk_or.Successful()) {
                ADD_FAILURE();
              } else if (chunk_or->has_value()) {
                matched_encrypted_ids_string += **chunk_or;
              }
              return SuccessExecutionResult();
            };
      });
  EXPECT_SUCCESS(matcher_.ExportMatches(
      {kPublisherBucketName, kPublisherMapping, kAdvertiserBucketName,
       kAdvertiserList, kOutputBucketName, kOutputList}));
  EXPECT_THAT(IdsStringToVector(matched_encrypted_ids_string),
              UnorderedElementsAre(kEncrypted1, kEncrypted3));
}

TEST_F(MatchWorkerTest, ExportPassesWipProvider) {
  EXPECT_CALL(*blob_storage_client_, GetBlobSync)
      .WillOnce([this](auto request) {
        EXPECT_EQ(request.blob_metadata().bucket_name(), kPublisherBucketName);
        EXPECT_EQ(request.blob_metadata().blob_name(), kPublisherMapping);
        EXPECT_EQ(request.cloud_identity_info().owner_id(),
                  "publisher_project");
        EXPECT_EQ(request.cloud_identity_info()
                      .attestation_info()
                      .gcp_attestation_info()
                      .wip_provider(),
                  "publisher_wip_provider");

        GetBlobResponse response;
        response.mutable_blob()->set_data(mapping_);
        return response;
      });

  EXPECT_CALL(blob_streamer_, GetBlobStream).WillOnce([this](auto context) {
    EXPECT_EQ(context.GetBucketName(), kAdvertiserBucketName);
    EXPECT_EQ(context.GetBlobPath(), kAdvertiserList);
    EXPECT_THAT(context.GetCloudIdentityInfo(),
                Optional(EqualsProto(BuildGcpCloudIdentityInfo(
                    "advertiser_project", "advertiser_wip_provider"))));

    CallCallbackWithEmails(context.GetCallback(), {kEmail1, kEmail3});
    return SuccessExecutionResult();
  });
  string matched_encrypted_ids_string;
  EXPECT_CALL(blob_streamer_, PutBlobStream)
      .WillOnce([this, &matched_encrypted_ids_string](auto context) {
        EXPECT_EQ(context.GetBucketName(), kOutputBucketName);
        EXPECT_EQ(context.GetBlobPath(), kOutputList);
        EXPECT_EQ(context.GetInitialData(), absl::StrCat(kEncrypted1, "\n"));
        EXPECT_THAT(context.GetCloudIdentityInfo(),
                    Optional(EqualsProto(BuildGcpCloudIdentityInfo(
                        "publisher_project", "publisher_wip_provider"))));
        matched_encrypted_ids_string += context.GetInitialData();

        // Expect all calls to be successful - record the chunks that come in.
        return
            [&matched_encrypted_ids_string](auto chunk_or) -> ExecutionResult {
              if (!chunk_or.Successful()) {
                ADD_FAILURE();
              } else if (chunk_or->has_value()) {
                matched_encrypted_ids_string += **chunk_or;
              }
              return SuccessExecutionResult();
            };
      });
  ExportMatchesRequest request{kPublisherBucketName,  kPublisherMapping,
                               kAdvertiserBucketName, kAdvertiserList,
                               kOutputBucketName,     kOutputList};
  request.publisher_cloud_identity_info =
      BuildGcpCloudIdentityInfo("publisher_project", "publisher_wip_provider");
  request.advertiser_cloud_identity_info = BuildGcpCloudIdentityInfo(
      "advertiser_project", "advertiser_wip_provider");
  EXPECT_SUCCESS(matcher_.ExportMatches(request));
  EXPECT_THAT(IdsStringToVector(matched_encrypted_ids_string),
              UnorderedElementsAre(kEncrypted1, kEncrypted3));
}

TEST_F(MatchWorkerTest, FailsIfGettingTheMappingFails) {
  EXPECT_CALL(*blob_storage_client_, GetBlobSync)
      .WillOnce(Return(FailureExecutionResult(12345)));

  EXPECT_CALL(blob_streamer_, GetBlobStream).Times(0);
  EXPECT_CALL(blob_streamer_, PutBlobStream).Times(0);
  EXPECT_THAT(matcher_.ExportMatches({kPublisherBucketName, kPublisherMapping,
                                      kAdvertiserBucketName, kAdvertiserList,
                                      kOutputBucketName, kOutputList}),
              ResultIs(FailureExecutionResult(12345)));
}

TEST_F(MatchWorkerTest, FailsIfGetBlobStreamFailsSync) {
  EXPECT_CALL(*blob_storage_client_, GetBlobSync).WillOnce([this](auto) {
    GetBlobResponse response;
    response.mutable_blob()->set_data(mapping_);
    return response;
  });

  EXPECT_CALL(blob_streamer_, GetBlobStream)
      .WillOnce(Return(FailureExecutionResult(12345)));
  EXPECT_CALL(blob_streamer_, PutBlobStream).Times(0);
  EXPECT_THAT(matcher_.ExportMatches({kPublisherBucketName, kPublisherMapping,
                                      kAdvertiserBucketName, kAdvertiserList,
                                      kOutputBucketName, kOutputList}),
              ResultIs(FailureExecutionResult(12345)));
}

TEST_F(MatchWorkerTest, FailsIfGetBlobStreamFailsAsync) {
  EXPECT_CALL(*blob_storage_client_, GetBlobSync)
      .WillOnce([this](auto request) {
        GetBlobResponse response;
        response.mutable_blob()->set_data(mapping_);
        return response;
      });

  EXPECT_CALL(blob_streamer_, GetBlobStream).WillOnce([this](auto context) {
    context.GetCallback()("", true, FailureExecutionResult(12345));
    return SuccessExecutionResult();
  });
  EXPECT_CALL(blob_streamer_, PutBlobStream).Times(0);
  EXPECT_THAT(matcher_.ExportMatches({kPublisherBucketName, kPublisherMapping,
                                      kAdvertiserBucketName, kAdvertiserList,
                                      kOutputBucketName, kOutputList}),
              ResultIs(FailureExecutionResult(12345)));
}

TEST_F(MatchWorkerTest, FailsIfGetBlobStreamFailureCancelsUpload) {
  EXPECT_CALL(*blob_storage_client_, GetBlobSync)
      .WillOnce([this](auto request) {
        GetBlobResponse response;
        response.mutable_blob()->set_data(mapping_);
        return response;
      });

  atomic_bool context_acquired(false);
  GetBlobStreamChunkProcessorCallback get_blob_stream_cb;
  EXPECT_CALL(blob_streamer_, GetBlobStream)
      .WillOnce(
          [this, &context_acquired, &get_blob_stream_cb](auto context) mutable {
            context.GetCallback()(absl::StrCat(kEmail1, "\n"), false,
                                  SuccessExecutionResult());
            get_blob_stream_cb = context.GetCallback();
            context_acquired = true;
            return SuccessExecutionResult();
          });
  atomic_int call_count(0);
  EXPECT_CALL(blob_streamer_, PutBlobStream)
      .WillOnce(Return([&call_count](auto chunk_or) -> ExecutionResult {
        call_count++;
        if (call_count.load() != 0) {
          EXPECT_THAT(chunk_or, ResultIs(FailureExecutionResult(12345)));
          return chunk_or.result();
        }
        return SuccessExecutionResult();
      }));
  // Spawn a thread that will wait until the PutBlobStream has been opened.
  thread listener_thread(
      [&context_acquired, &call_count, &get_blob_stream_cb]() {
        WaitUntil([&context_acquired]() { return context_acquired.load(); });
        // Sleep some to ensure that the main thread had time to open the
        // PutBlobStream.
        sleep_for(milliseconds(100));
        get_blob_stream_cb("", true, FailureExecutionResult(12345));
      });
  EXPECT_THAT(matcher_.ExportMatches({kPublisherBucketName, kPublisherMapping,
                                      kAdvertiserBucketName, kAdvertiserList,
                                      kOutputBucketName, kOutputList}),
              ResultIs(FailureExecutionResult(12345)));
  EXPECT_EQ(call_count.load(), 1);
  listener_thread.join();
}

TEST_F(MatchWorkerTest, FailsIfPutBlobStreamFails) {
  EXPECT_CALL(*blob_storage_client_, GetBlobSync)
      .WillOnce([this](auto request) {
        GetBlobResponse response;
        response.mutable_blob()->set_data(mapping_);
        return response;
      });

  EXPECT_CALL(blob_streamer_, GetBlobStream)
      .WillOnce([this](auto context) mutable {
        CallCallbackWithEmails(context.GetCallback(), {kEmail1, kEmail3});
        return SuccessExecutionResult();
      });
  EXPECT_CALL(blob_streamer_, PutBlobStream)
      .WillOnce(Return(FailureExecutionResult(12345)));
  EXPECT_THAT(matcher_.ExportMatches({kPublisherBucketName, kPublisherMapping,
                                      kAdvertiserBucketName, kAdvertiserList,
                                      kOutputBucketName, kOutputList}),
              ResultIs(FailureExecutionResult(12345)));
}

}  // namespace google::pair::matcher::test
