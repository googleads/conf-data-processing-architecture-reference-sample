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

#include "cc/matcher/match_table/src/match_table_hash_map.h"

#include <gtest/gtest.h>

#include <string>

#include "absl/container/flat_hash_map.h"
#include "cc/matcher/match_table/src/error_codes.h"
#include "cc/public/core/interface/execution_result.h"
#include "cc/public/core/test/interface/execution_result_matchers.h"

using absl::flat_hash_map;
using google::pair::matcher::errors::MATCH_TABLE_ELEMENT_ALREADY_EXISTS;
using google::pair::matcher::errors::MATCH_TABLE_ELEMENT_DOES_NOT_EXIST;
using google::scp::core::FailureExecutionResult;
using google::scp::core::test::IsSuccessfulAndHolds;
using google::scp::core::test::ResultIs;
using std::string;
using testing::Pair;
using testing::UnorderedElementsAre;

namespace google::pair::matcher::test {

TEST(MatchTableHashMapTest, ShouldSuccessfullyAddElement) {
  MatchTableHashMap<string, string> table;

  EXPECT_SUCCESS(table.AddElement("key", "value"));
}

TEST(MatchTableHashMapTest, ShouldGetValueIfElementMarkedAsMatchedExists) {
  MatchTableHashMap<string, string> table;
  EXPECT_SUCCESS(table.AddElement("key", "value"));

  EXPECT_THAT(table.MarkMatched("key"), IsSuccessfulAndHolds("value"));
}

TEST(MatchTableHashMapTest, AddingShouldFailIfElementAlreadyExists) {
  MatchTableHashMap<string, string> table;
  EXPECT_SUCCESS(table.AddElement("key", "value"));

  EXPECT_THAT(
      table.AddElement("key", "value"),
      ResultIs(FailureExecutionResult(MATCH_TABLE_ELEMENT_ALREADY_EXISTS)));
}

TEST(MatchTableHashMapTest, MarkingMatchedShouldFailIfElementDoesNotExist) {
  MatchTableHashMap<string, string> table;

  EXPECT_THAT(
      table.MarkMatched("key"),
      ResultIs(FailureExecutionResult(MATCH_TABLE_ELEMENT_DOES_NOT_EXIST)));
}

TEST(MatchTableHashMapTest, ShouldBeAbleToAddAndMarkMatchedMultipleElements) {
  MatchTableHashMap<string, string> table;

  EXPECT_SUCCESS(table.AddElement("key1", "value1"));
  EXPECT_SUCCESS(table.AddElement("key2", "value2"));

  EXPECT_THAT(table.MarkMatched("key1"), IsSuccessfulAndHolds("value1"));
  EXPECT_THAT(table.MarkMatched("key2"), IsSuccessfulAndHolds("value2"));
}

TEST(MatchTableHashMapTest, VisitorShouldGetCalledWithAllMatchedElements) {
  MatchTableHashMap<string, string> table;

  // Add 5 elements
  EXPECT_SUCCESS(table.AddElement("key1", "value1"));
  EXPECT_SUCCESS(table.AddElement("key2", "value2"));
  EXPECT_SUCCESS(table.AddElement("key3", "value3"));
  EXPECT_SUCCESS(table.AddElement("key4", "value4"));
  EXPECT_SUCCESS(table.AddElement("key5", "value5"));

  // Only mark 3 as matched
  EXPECT_SUCCESS(table.MarkMatched("key1"));
  EXPECT_SUCCESS(table.MarkMatched("key4"));
  EXPECT_SUCCESS(table.MarkMatched("key5"));

  flat_hash_map<string, string> matched_items;

  table.VisitMatched(
      [&matched_items](const auto& k, const auto& v) { matched_items[k] = v; });

  EXPECT_EQ(3, matched_items.size());
  EXPECT_THAT(matched_items, UnorderedElementsAre(Pair("key1", "value1"),
                                                  Pair("key4", "value4"),
                                                  Pair("key5", "value5")));
}

TEST(MatchTableHashMapTest, VisitorShouldNotGetCalledIfElementsWereNotMatched) {
  MatchTableHashMap<string, string> table;

  // Add 5 elements
  EXPECT_SUCCESS(table.AddElement("key1", "value1"));
  EXPECT_SUCCESS(table.AddElement("key2", "value2"));
  EXPECT_SUCCESS(table.AddElement("key3", "value3"));
  EXPECT_SUCCESS(table.AddElement("key4", "value4"));
  EXPECT_SUCCESS(table.AddElement("key5", "value5"));

  table.VisitMatched([](const auto& k, const auto& v) {
    FAIL() << "Did not expect visitor to be called";
  });
}

}  // namespace google::pair::matcher::test
