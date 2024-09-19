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

#include "blob_streamer.h"

#include <atomic>
#include <memory>
#include <string_view>

#include "get_blob_stream_context.h"

using google::cmrt::sdk::blob_storage_service::v1::GetBlobStreamRequest;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobStreamResponse;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobStreamRequest;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobStreamResponse;
using google::pair::common::GetBlobStreamContext;
using google::pair::common::PutBlobStreamContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::AsyncPriority;
using google::scp::core::ConsumerStreamingContext;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::ProducerStreamingContext;
using google::scp::core::SuccessExecutionResult;
using google::scp::cpio::BlobStorageClientInterface;
using std::atomic;
using std::bind;
using std::make_shared;
using std::move;
using std::optional;
using std::shared_ptr;
using std::string;
using std::string_view;
using std::unique_ptr;
using std::placeholders::_1;

namespace {

// Builds a ConsumerStreamingContext with just the request set from the given
// GetBlobStreamContext.
ConsumerStreamingContext<GetBlobStreamRequest, GetBlobStreamResponse>
BuildGetBlobStreamingContext(GetBlobStreamContext& get_blob_context) {
  auto get_blob_stream_request = make_shared<GetBlobStreamRequest>();
  get_blob_stream_request->mutable_blob_metadata()->set_bucket_name(
      get_blob_context.GetBucketName());
  get_blob_stream_request->mutable_blob_metadata()->set_blob_name(
      get_blob_context.GetBlobPath());
  get_blob_stream_request->set_max_bytes_per_response(
      get_blob_context.GetMaxBytesPerChunk());
  if (get_blob_context.GetCloudIdentityInfo()) {
    *get_blob_stream_request->mutable_cloud_identity_info() =
        move(*get_blob_context.GetCloudIdentityInfo());
  }

  ConsumerStreamingContext<GetBlobStreamRequest, GetBlobStreamResponse>
      get_blob_stream_context;
  get_blob_stream_context.request = move(get_blob_stream_request);

  return get_blob_stream_context;
}

// Builds a ProducerStreamingContext with just the request set from the given
// PutBlobStreamContext.
ProducerStreamingContext<PutBlobStreamRequest, PutBlobStreamResponse>
BuildPutBlobStreamingContext(PutBlobStreamContext& put_blob_context) {
  auto put_blob_stream_request = make_shared<PutBlobStreamRequest>();
  put_blob_stream_request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_bucket_name(put_blob_context.GetBucketName());
  put_blob_stream_request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_blob_name(put_blob_context.GetBlobPath());
  put_blob_stream_request->mutable_blob_portion()->set_data(
      move(put_blob_context.GetInitialData()));
  if (put_blob_context.GetCloudIdentityInfo()) {
    *put_blob_stream_request->mutable_cloud_identity_info() =
        move(*put_blob_context.GetCloudIdentityInfo());
  }

  ProducerStreamingContext<PutBlobStreamRequest, PutBlobStreamResponse>
      put_blob_stream_context;
  put_blob_stream_context.request = move(put_blob_stream_request);

  return put_blob_stream_context;
}

/// @brief A function which, when bound with the proper arguments, accepts
/// multiple more_data_or arguments and pushes them onto the upload.
/// @param put_blob_stream_context context for uploading the blob
/// @param is_done flag indicating whether put_blob_stream_context is completely
/// done.
/// @param result when is_done.load() == true, contains the result of the upload
/// completing.
/// @param more_data_or Supplied by the caller: more data to upload, indicator
/// to finish the upload, or indicator to cancel the upload.
ExecutionResult PutBlobStreamFunctor(
    ProducerStreamingContext<PutBlobStreamRequest, PutBlobStreamResponse>
        put_blob_stream_context,
    shared_ptr<atomic<bool>> is_done, shared_ptr<ExecutionResult> result,
    ExecutionResultOr<optional<string>> more_data_or) {
  if (!more_data_or.Successful()) {
    // Cancel the upload.
    put_blob_stream_context.TryCancel();
    while (!is_done->load());
    return *result;
  }
  if (!more_data_or->has_value()) {
    // Finish the upload.
    put_blob_stream_context.MarkDone();
    while (!is_done->load());
    return *result;
  }
  // Push the data onto the upload.
  PutBlobStreamRequest next_request;
  *next_request.mutable_blob_portion()->mutable_metadata() =
      put_blob_stream_context.request->blob_portion().metadata();
  string& data = **more_data_or;
  next_request.mutable_blob_portion()->set_data(move(data));
  auto push_result = put_blob_stream_context.TryPushRequest(move(next_request));
  if (!push_result.Successful()) {
    // If pushing fails, try to cancel and acquire the result.
    put_blob_stream_context.TryCancel();
    while (!is_done->load());
    return *result;
  }
  return SuccessExecutionResult();
}

}  // namespace

namespace google::pair::common {

BlobStreamer::BlobStreamer(
    shared_ptr<AsyncExecutorInterface> async_executor,
    shared_ptr<BlobStorageClientInterface> blob_storage_client)
    : async_executor_(async_executor),
      blob_storage_client_(blob_storage_client),
      stop_(false) {}

ExecutionResult BlobStreamer::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult BlobStreamer::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult BlobStreamer::Stop() noexcept {
  stop_.store(true);
  return SuccessExecutionResult();
}

ExecutionResult BlobStreamer::GetBlobStream(
    GetBlobStreamContext get_blob_context) noexcept {
  auto get_blob_stream_context = BuildGetBlobStreamingContext(get_blob_context);

  auto result = make_shared<ExecutionResult>();
  auto is_done = make_shared<atomic<bool>>(false);

  get_blob_stream_context.process_callback =
      [result, is_done](auto& context, bool stream_done) {
        // We do nothing with the enqueued data, just wait until we're done
        // streaming. Enqueued data is processed asynchronously on another
        // thread.
        if (stream_done) {
          *result = context.result;
          is_done->store(true);
        }
      };

  blob_storage_client_->GetBlobStream(get_blob_stream_context);

  // TODO: This will hijack one thread for the entire duration of the stream, so
  // we might want to look into adding a dedicated thread in the future or
  // switching to handling stream chunks in multiple threads.
  return async_executor_->Schedule(
      [result, is_done, get_blob_stream_context,
       callback = get_blob_context.GetCallback(), this]() mutable {
        while (!stop_.load()) {
          auto response = get_blob_stream_context.TryGetNextResponse();

          if (response == nullptr && get_blob_stream_context.IsMarkedDone()) {
            // It's possible response can be nullptr but a response is pushed
            // and the queue marked done before we check it. Catch that edge
            // case here.
            response = get_blob_stream_context.TryGetNextResponse();
            // This signals we're done with the stream.
            if (response == nullptr && is_done->load()) {
              // Just return empty data. We actually return to exit out of this
              // loop.
              return callback(string_view(), /* is_done */ true, *result);
            } else if (response != nullptr) {
              callback(response->blob_portion().data(),
                       /* is done */ false, SuccessExecutionResult());
            }
          } else if (response != nullptr) {
            callback(response->blob_portion().data(), /* is_done */ false,
                     SuccessExecutionResult());
          }
        }
      },
      AsyncPriority::Normal);
}

ExecutionResultOr<PutBlobCallback> BlobStreamer::PutBlobStream(
    PutBlobStreamContext put_blob_context) noexcept {
  auto put_blob_stream_context = BuildPutBlobStreamingContext(put_blob_context);

  auto result = make_shared<ExecutionResult>();
  auto is_done = make_shared<atomic<bool>>(false);

  put_blob_stream_context.callback = [result, is_done](auto& context) {
    *result = context.result;
    is_done->store(true);
  };

  blob_storage_client_->PutBlobStream(put_blob_stream_context);

  return bind(&PutBlobStreamFunctor, put_blob_stream_context, is_done, result,
              _1);
}
}  // namespace google::pair::common