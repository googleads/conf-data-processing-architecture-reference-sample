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

#include <gmock/gmock.h>

#include "cc/public/core/interface/execution_result.h"
#include "cc/publisher_list_generator/publisher_list_fetcher/src/publisher_list_fetcher.h"

namespace google::pair::publisher_list_generator {

class MockPublisherListFetcher : public PublisherListFetcher {
 public:
  MOCK_METHOD(google::scp::core::ExecutionResultOr<FetchIdsResponse>,
              FetchPublisherIds, (FetchIdsRequest), (override));
};

}  // namespace google::pair::publisher_list_generator
