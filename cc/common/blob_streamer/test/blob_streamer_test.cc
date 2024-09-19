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

#include "cc/common/blob_streamer/src/blob_streamer.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "cc/common/attestation/src/attestation_info.h"
#include "cc/core/async_executor/src/async_executor.h"
#include "cc/core/test/utils/auto_init_run_stop.h"
#include "cc/public/core/interface/execution_result.h"
#include "cc/public/core/test/interface/execution_result_matchers.h"
#include "cc/public/cpio/mock/blob_storage_client/mock_blob_storage_client.h"
#include "core/test/utils/conditional_wait.h"
#include "public/cpio/interface/blob_storage_client/blob_storage_client_interface.h"

using google::cmrt::sdk::blob_storage_service::v1::DeleteBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::DeleteBlobResponse;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobResponse;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobStreamRequest;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobStreamResponse;
using google::cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataRequest;
using google::cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataResponse;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobResponse;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobStreamRequest;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobStreamResponse;
using google::pair::common::BuildGcpCloudIdentityInfo;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutor;
using google::scp::core::ConsumerStreamingContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::ProducerStreamingContext;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::test::AutoInitRunStop;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::BlobStorageClientInterface;
using google::scp::cpio::MockBlobStorageClient;
using std::atomic;
using std::lock_guard;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::string;
using std::thread;
using std::unique_ptr;
using std::vector;
using testing::ElementsAre;
using testing::NiceMock;
using testing::Return;

namespace google::pair::common::test {

class BlobStreamerTest : public ::testing::Test {
 protected:
  BlobStreamerTest()
      : async_executor_(make_shared<AsyncExecutor>(1, 1)),
        storage_client_mock_(*new NiceMock<MockBlobStorageClient>()),
        streamer_(async_executor_,
                  shared_ptr<MockBlobStorageClient>(&storage_client_mock_)) {
    EXPECT_SUCCESS(async_executor_->Init());
    EXPECT_SUCCESS(async_executor_->Run());
  }

  ~BlobStreamerTest() { EXPECT_SUCCESS(async_executor_->Stop()); }

  shared_ptr<AsyncExecutor> async_executor_;
  MockBlobStorageClient& storage_client_mock_;
  BlobStreamer streamer_;
};

static void MarkStreamDone(
    ConsumerStreamingContext<GetBlobStreamRequest, GetBlobStreamResponse>&
        get_blob_streaming_context,
    const ExecutionResult& result = SuccessExecutionResult()) {
  get_blob_streaming_context.result = result;
  get_blob_streaming_context.MarkDone();
  get_blob_streaming_context.process_callback(get_blob_streaming_context,
                                              /* is_done */ true);
}

TEST_F(BlobStreamerTest,
       GetBlob_ShouldUseContextInformationToBuildStreamingContext) {
  auto get_blob_context = GetBlobStreamContext(
      /* bucket_name */ "test-bucket", /* blob_path */ "test-file",
      /* max_bytes_per_chunk */ 123,
      [](auto chunk, bool is_done, const auto& result) {});
  EXPECT_CALL(storage_client_mock_, GetBlobStream).WillOnce([](auto context) {
    MarkStreamDone(context);

    EXPECT_EQ(context.request->blob_metadata().bucket_name(), "test-bucket");
    EXPECT_EQ(context.request->blob_metadata().blob_name(), "test-file");
    EXPECT_EQ(context.request->max_bytes_per_response(), 123);
  });
  EXPECT_SUCCESS(streamer_.Init());
  EXPECT_SUCCESS(streamer_.Run());

  EXPECT_SUCCESS(streamer_.GetBlobStream(get_blob_context));

  EXPECT_SUCCESS(streamer_.Stop());
}

static void AddDataChunkToStream(
    ConsumerStreamingContext<GetBlobStreamRequest, GetBlobStreamResponse>&
        get_blob_streaming_context,
    const string& data) {
  GetBlobStreamResponse response;
  response.mutable_blob_portion()->set_data(data);
  EXPECT_SUCCESS(get_blob_streaming_context.TryPushResponse(response));
}

TEST_F(BlobStreamerTest, GetBlob_ShouldPassPayloadToContextCallback) {
  // Capture the data chunks here
  vector<string> data_chunks;
  atomic<bool> is_finished{false};
  auto get_blob_context = GetBlobStreamContext(
      /* bucket_name */ "test-bucket", /* blob_path */ "test-file",
      /* max_bytes_per_chunk */ 123,
      [&data_chunks, &is_finished](auto chunk, bool is_done,
                                   const auto& result) {
        if (is_done) {
          is_finished.store(true);
        } else {
          data_chunks.emplace_back(chunk);
        }
      });
  EXPECT_CALL(storage_client_mock_, GetBlobStream).WillOnce([](auto context) {
    EXPECT_FALSE(context.request->has_cloud_identity_info())
        << context.request->cloud_identity_info().DebugString();
    AddDataChunkToStream(context, "hello");
    AddDataChunkToStream(context, "world");
    MarkStreamDone(context);
  });
  EXPECT_SUCCESS(streamer_.Init());
  EXPECT_SUCCESS(streamer_.Run());

  EXPECT_SUCCESS(streamer_.GetBlobStream(get_blob_context));

  WaitUntil([&is_finished]() { return is_finished.load(); });
  // Expect that the data that was passed by the stream was also passed to
  // the context callback
  EXPECT_THAT(data_chunks, ElementsAre("hello", "world"));
  EXPECT_SUCCESS(streamer_.Stop());
}

TEST_F(BlobStreamerTest, GetBlob_PassesWipProvider) {
  // Capture the data chunks here
  vector<string> data_chunks;
  atomic<bool> is_finished{false};
  auto get_blob_context = GetBlobStreamContext(
      /* bucket_name */
      "test-bucket", /* blob_path */ "test-file",
      /* max_bytes_per_chunk */ 123,
      [&data_chunks, &is_finished](auto chunk, bool is_done,
                                   const auto& result) {
        if (is_done) {
          is_finished.store(true);
        } else {
          data_chunks.emplace_back(chunk);
        }
      },
      BuildGcpCloudIdentityInfo("project", "wip_provider"));
  EXPECT_CALL(storage_client_mock_, GetBlobStream).WillOnce([](auto context) {
    EXPECT_EQ(context.request->cloud_identity_info().owner_id(), "project");
    EXPECT_EQ(context.request->cloud_identity_info()
                  .attestation_info()
                  .gcp_attestation_info()
                  .wip_provider(),
              "wip_provider");
    AddDataChunkToStream(context, "hello");
    AddDataChunkToStream(context, "world");
    MarkStreamDone(context);
  });
  EXPECT_SUCCESS(streamer_.Init());
  EXPECT_SUCCESS(streamer_.Run());

  EXPECT_SUCCESS(streamer_.GetBlobStream(get_blob_context));

  WaitUntil([&is_finished]() { return is_finished.load(); });
  // Expect that the data that was passed by the stream was also passed to
  // the context callback
  EXPECT_THAT(data_chunks, ElementsAre("hello", "world"));
  EXPECT_SUCCESS(streamer_.Stop());
}

TEST_F(BlobStreamerTest, GetBlob_ShouldPassResultToContextCallback) {
  atomic<bool> is_finished{false};
  // Capture the result here
  ExecutionResult stream_result;
  auto get_blob_context = GetBlobStreamContext(
      /* bucket_name */ "test-bucket", /* blob_path */ "test-file",
      /* max_bytes_per_chunk */ 123,
      [&is_finished, &stream_result](auto chunk, bool is_done,
                                     const auto& result) {
        stream_result = result;
        is_finished.store(is_done);
      });
  EXPECT_CALL(storage_client_mock_, GetBlobStream).WillOnce([](auto context) {
    MarkStreamDone(context, FailureExecutionResult(1234554321));
  });
  EXPECT_SUCCESS(streamer_.Init());
  EXPECT_SUCCESS(streamer_.Run());

  EXPECT_SUCCESS(streamer_.GetBlobStream(get_blob_context));

  WaitUntil([&is_finished]() { return is_finished.load(); });
  EXPECT_THAT(stream_result, ResultIs(FailureExecutionResult(1234554321)));
  EXPECT_SUCCESS(streamer_.Stop());
}

TEST_F(BlobStreamerTest,
       PutBlob_ShouldUseContextInformationToBuildStreamingContext) {
  auto put_blob_context = PutBlobStreamContext(
      /* bucket_name */ "test-bucket", /* blob_path */ "test-file",
      /* initial_data */ "some-data");
  EXPECT_CALL(storage_client_mock_, PutBlobStream).WillOnce([](auto context) {
    EXPECT_EQ(context.request->blob_portion().metadata().bucket_name(),
              "test-bucket");
    EXPECT_EQ(context.request->blob_portion().metadata().blob_name(),
              "test-file");
    EXPECT_EQ(context.request->blob_portion().data(), "some-data");
  });
  EXPECT_SUCCESS(streamer_.Init());
  EXPECT_SUCCESS(streamer_.Run());

  EXPECT_SUCCESS(streamer_.PutBlobStream(put_blob_context));

  EXPECT_SUCCESS(streamer_.Stop());
}

void SpawnThreadToReadRequests(
    ProducerStreamingContext<PutBlobStreamRequest, PutBlobStreamResponse>
        context,
    vector<string>& more_payloads, ExecutionResult result_to_store) {
  thread reader_thread([context, &more_payloads, result_to_store]() mutable {
    unique_ptr<PutBlobStreamRequest> next_request;
    while (!context.IsMarkedDone() && !context.IsCancelled()) {
      next_request = context.TryGetNextRequest();
      if (next_request != nullptr) {
        more_payloads.emplace_back(next_request->blob_portion().data());
      }
    }
    next_request = context.TryGetNextRequest();
    while (next_request != nullptr) {
      more_payloads.emplace_back(next_request->blob_portion().data());
      next_request = context.TryGetNextRequest();
    }
    context.result = result_to_store;
    context.Finish();
  });
  reader_thread.detach();
}

TEST_F(BlobStreamerTest, PutBlob_ShouldReturnFunctionWhichPassesDataToContext) {
  auto put_blob_context = PutBlobStreamContext(
      /* bucket_name */ "test-bucket", /* blob_path */ "test-file",
      /* initial_data */ "some-data");
  vector<string> more_payloads;
  EXPECT_CALL(storage_client_mock_, PutBlobStream)
      .WillOnce([&more_payloads](auto context) {
        EXPECT_EQ(context.request->blob_portion().metadata().bucket_name(),
                  "test-bucket");
        EXPECT_EQ(context.request->blob_portion().metadata().blob_name(),
                  "test-file");
        EXPECT_EQ(context.request->blob_portion().data(), "some-data");
        EXPECT_FALSE(context.request->has_cloud_identity_info())
            << context.request->cloud_identity_info().DebugString();

        SpawnThreadToReadRequests(context, more_payloads,
                                  SuccessExecutionResult());
      });
  EXPECT_SUCCESS(streamer_.Init());
  EXPECT_SUCCESS(streamer_.Run());

  ASSERT_SUCCESS_AND_ASSIGN(auto func,
                            streamer_.PutBlobStream(put_blob_context));

  ASSERT_SUCCESS(func("more-data"));
  ASSERT_SUCCESS(func("even-more-data"));
  ASSERT_SUCCESS(func(PutBlobStreamDoneMarker));
  EXPECT_THAT(more_payloads, ElementsAre("more-data", "even-more-data"));

  EXPECT_SUCCESS(streamer_.Stop());
}

TEST_F(BlobStreamerTest, PutBlob_PassesWipProvider) {
  auto put_blob_context = PutBlobStreamContext(
      /* bucket_name */ "test-bucket", /* blob_path */ "test-file",
      /* initial_data */ "some-data",
      BuildGcpCloudIdentityInfo("project", "wip_provider"));
  vector<string> more_payloads;
  EXPECT_CALL(storage_client_mock_, PutBlobStream)
      .WillOnce([&more_payloads](auto context) {
        EXPECT_EQ(context.request->blob_portion().metadata().bucket_name(),
                  "test-bucket");
        EXPECT_EQ(context.request->blob_portion().metadata().blob_name(),
                  "test-file");
        EXPECT_EQ(context.request->blob_portion().data(), "some-data");
        EXPECT_EQ(context.request->cloud_identity_info().owner_id(), "project");
        EXPECT_EQ(context.request->cloud_identity_info()
                      .attestation_info()
                      .gcp_attestation_info()
                      .wip_provider(),
                  "wip_provider");

        SpawnThreadToReadRequests(context, more_payloads,
                                  SuccessExecutionResult());
      });
  EXPECT_SUCCESS(streamer_.Init());
  EXPECT_SUCCESS(streamer_.Run());

  ASSERT_SUCCESS_AND_ASSIGN(auto func,
                            streamer_.PutBlobStream(put_blob_context));

  ASSERT_SUCCESS(func("more-data"));
  ASSERT_SUCCESS(func("even-more-data"));
  ASSERT_SUCCESS(func(PutBlobStreamDoneMarker));
  EXPECT_THAT(more_payloads, ElementsAre("more-data", "even-more-data"));

  EXPECT_SUCCESS(streamer_.Stop());
}

void SpawnThreadToWaitForCancellation(
    ProducerStreamingContext<PutBlobStreamRequest, PutBlobStreamResponse>
        context,
    ExecutionResult result_to_store) {
  thread reader_thread([context, result_to_store]() mutable {
    WaitUntil([&context]() { return context.IsCancelled(); });
    context.result = result_to_store;
    context.Finish();
  });
  reader_thread.detach();
}

TEST_F(BlobStreamerTest, PutBlob_CancellingWorks) {
  auto put_blob_context = PutBlobStreamContext(
      /* bucket_name */ "test-bucket", /* blob_path */ "test-file",
      /* initial_data */ "some-data");
  EXPECT_CALL(storage_client_mock_, PutBlobStream).WillOnce([](auto context) {
    EXPECT_EQ(context.request->blob_portion().metadata().bucket_name(),
              "test-bucket");
    EXPECT_EQ(context.request->blob_portion().metadata().blob_name(),
              "test-file");
    EXPECT_EQ(context.request->blob_portion().data(), "some-data");

    SpawnThreadToWaitForCancellation(context, FailureExecutionResult(12345));
  });
  EXPECT_SUCCESS(streamer_.Init());
  EXPECT_SUCCESS(streamer_.Run());

  ASSERT_SUCCESS_AND_ASSIGN(auto func,
                            streamer_.PutBlobStream(put_blob_context));

  EXPECT_THAT(func(FailureExecutionResult(678910)),
              ResultIs(FailureExecutionResult(12345)));

  EXPECT_SUCCESS(streamer_.Stop());
}

}  // namespace google::pair::common::test
