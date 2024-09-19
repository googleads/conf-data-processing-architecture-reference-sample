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

#include "cc/publisher_list_generator/publisher_list_fetcher/src/local_publisher_list_fetcher.h"

#include <gtest/gtest.h>

#include <string>

#include "cc/public/core/interface/execution_result.h"
#include "cc/public/core/test/interface/execution_result_matchers.h"
#include "cc/publisher_list_generator/publisher_list_fetcher/src/error_codes.h"

using google::scp::core::FailureExecutionResult;
using google::scp::core::test::ResultIs;
using std::move;
using testing::UnorderedElementsAre;

namespace {
constexpr char kTestFilePath[] =
    "cc/publisher_list_generator/publisher_list_fetcher/test/test.csv";
constexpr char kNonExistentFilePath[] = "cc/some/nonexistent/file.csv";
}  // namespace

namespace google::pair::publisher_list_generator::test {

TEST(LocalPublisherListFetcherTest, FetchesItemsOutOfFile) {
  LocalPublisherListFetcher fetcher;
  ASSERT_SUCCESS_AND_ASSIGN(
      auto fetch_resp,
      fetcher.FetchPublisherIds(FetchIdsRequest{kTestFilePath}));
  EXPECT_THAT(fetch_resp.ids, UnorderedElementsAre(
                                  "someone@gmail.com", "someone.else@yahoo.com",
                                  "yet.another.person@hotmail.com"));
}

TEST(LocalPublisherListFetcherTest, FailsIfFileDoesNotExist) {
  LocalPublisherListFetcher fetcher;
  EXPECT_THAT(fetcher.FetchPublisherIds(FetchIdsRequest{kNonExistentFilePath}),
              ResultIs(FailureExecutionResult(
                  errors::PUBLISHER_LIST_FETCHER_ERROR_OPENING_FILE)));
}

}  // namespace google::pair::publisher_list_generator::test
