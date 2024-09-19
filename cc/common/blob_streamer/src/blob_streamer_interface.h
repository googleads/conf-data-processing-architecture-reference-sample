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

#include <functional>
#include <string_view>

#include "cc/core/interface/service_interface.h"
#include "cc/public/core/interface/execution_result.h"

#include "get_blob_stream_context.h"
#include "put_blob_stream_context.h"

namespace google::pair::common {

/**
 * @brief A function the caller can call with more data to upload on the stream.
 * To complete the upload, pass in a PutBlobStreamDoneMarker.
 * When the function is called with PutBlobStreamDoneMarker, it will block until
 * the upload is done and then return the status of the upload. To indicate an
 * error has occurred and the upload should be cancelled, input a
 * FailureExecutionResult.
 *
 */
using PutBlobCallback = std::function<scp::core::ExecutionResult(
    scp::core::ExecutionResultOr<std::optional<std::string>>)>;

/**
 * @brief Alias for the marker to pass in to a PutBlobCallback to indicate the
 * stream is done.
 *
 */
constexpr auto PutBlobStreamDoneMarker = std::nullopt;

/**
 * @brief Interface for a blob streamer class.
 *
 */
class BlobStreamerInterface : public scp::core::ServiceInterface {
 public:
  /**
   * @brief Start a blob download streaming flow.
   *
   * @param get_blob_context The context to use to get the blob object in a
   * streaming fashion
   * @return scp::core::ExecutionResult
   */
  virtual scp::core::ExecutionResult GetBlobStream(
      GetBlobStreamContext get_blob_context) noexcept = 0;

  /**
   * @brief Start a blob upload streaming flow.
   *
   * @param put_blob_context The context to use to put the blob object in a
   * streaming fashion
   * @return scp::core::ExecutionResultOr<PutBlobCallback> The callback to use
   * to upload more data to the stream - or a failure.
   */
  virtual scp::core::ExecutionResultOr<PutBlobCallback> PutBlobStream(
      PutBlobStreamContext put_blob_context) noexcept = 0;
};

}  // namespace google::pair::common
