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

#include "cc/common/csv_parser/src/csv_row.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "absl/strings/str_join.h"
#include "cc/common/csv_parser/src/error_codes.h"
#include "cc/public/core/test/interface/execution_result_matchers.h"

using absl::StrCat;
using absl::StrJoin;
using google::pair::common::CsvRow;
using google::pair::common::errors::CSV_COL_INDEX_OUT_OF_BOUNDS;
using google::pair::common::errors::CSV_ROW_UNEXPECTED_NUMBER_OF_COLUMNS;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::test::IsSuccessfulAndHolds;
using google::scp::core::test::ResultIs;
using std::string;
using std::transform;
using std::vector;
using testing::ElementsAreArray;
using testing::ExplainMatchResult;

namespace google::pair::common::test {

MATCHER_P(RowHasExactColumns, expected_columns, "") {
  const CsvRow& row = arg;
  auto i = 0;
  vector<string> actual_columns;

  for (auto col_or = row.GetColumn(i); col_or.Successful();
       col_or = row.GetColumn(i)) {
    actual_columns.emplace_back(col_or.release());
    i++;
  }

  auto result = ExplainMatchResult(ElementsAreArray(expected_columns),
                                   actual_columns, result_listener);
  if (!result) {
    vector<string> printable_actual_columns(actual_columns.size());
    transform(actual_columns.begin(), actual_columns.end(),
              printable_actual_columns.begin(),
              [](const string& str) { return StrCat("\"", str, "\""); });
    *result_listener << ": { " << StrJoin(printable_actual_columns, ", ")
                     << " }";
  }
  return result;
}

TEST(CsvRowTest, BuildShouldParseLineSuccessfully) {
  auto line = "val1,val2,val3";

  EXPECT_SUCCESS(CsvRow::Build(line, /* num_cols */ 3,
                               /* remove_whitespace */ true,
                               /* delimiter */ ','));
}

TEST(CsvRowTest, BuildShouldHandleSingleRows) {
  auto line = "   val1  ";

  ASSERT_SUCCESS_AND_ASSIGN(auto row,
                            CsvRow::Build(line, /* num_cols */ 1,
                                          /* remove_whitespace */ true,
                                          /* delimiter */ ','));

  EXPECT_THAT(row.GetColumn(0), IsSuccessfulAndHolds("val1"));
}

TEST(CsvRowTest,
     BuildParsingShouldFailIfLengthDoesNotMatchTheExpectedDueToInput) {
  auto line = "val1,val2";

  EXPECT_THAT(
      CsvRow::Build(line, /* num_cols */ 3,
                    /* remove_whitespace */ true,
                    /* delimiter */ ','),
      ResultIs(FailureExecutionResult(CSV_ROW_UNEXPECTED_NUMBER_OF_COLUMNS)));
}

TEST(CsvRowTest,
     BuildParsingShouldFailIfLengthDoesNotMatchTheExpectedDueToConfig) {
  auto line = "val1,val2,val3";

  EXPECT_THAT(
      CsvRow::Build(line, /* num_cols */ 2,
                    /* remove_whitespace */ true,
                    /* delimiter */ ','),
      ResultIs(FailureExecutionResult(CSV_ROW_UNEXPECTED_NUMBER_OF_COLUMNS)));
}

TEST(CsvRowTest,
     BuildParsingShouldFailIfLengthDoesNotMatchTheExpectedDueToDelimiter) {
  auto line = "val1,val2,val3";

  EXPECT_THAT(
      CsvRow::Build(line, /* num_cols */ 3,
                    /* remove_whitespace */ true,
                    /* delimiter */ '-'),
      ResultIs(FailureExecutionResult(CSV_ROW_UNEXPECTED_NUMBER_OF_COLUMNS)));
}

TEST(CsvRowTest, ElementsInRowShouldBeAccessibleAfterBuild) {
  auto line = "val1,val2,val3";
  ASSERT_SUCCESS_AND_ASSIGN(auto row,
                            CsvRow::Build(line, /* num_cols */ 3,
                                          /* remove_whitespace */ true,
                                          /* delimiter */ ','));

  EXPECT_THAT(row,
              RowHasExactColumns(vector<string>({"val1", "val2", "val3"})));
}

TEST(CsvRowTest, BuildShouldRemoveWhitespace) {
  auto line = "  val1   ,\t    val2,  val3   ";
  ASSERT_SUCCESS_AND_ASSIGN(auto row,
                            CsvRow::Build(line, /* num_cols */ 3,
                                          /* remove_whitespace */ true,
                                          /* delimiter */ ','));

  EXPECT_THAT(row,
              RowHasExactColumns(vector<string>({"val1", "val2", "val3"})));
}

TEST(CsvRowTest, BuildShouldNotRemoveWhitespace) {
  auto line = "  val1   ,    val2,  val3   ";
  ASSERT_SUCCESS_AND_ASSIGN(auto row,
                            CsvRow::Build(line, /* num_cols */ 3,
                                          /* remove_whitespace */ false,
                                          /* delimiter */ ','));

  EXPECT_THAT(row, RowHasExactColumns(
                       vector<string>({"  val1   ", "    val2", "  val3   "})));
}

TEST(CsvRowTest, BuildShouldHandleEmptyStringWithFailure) {
  auto line = "";
  EXPECT_THAT(
      CsvRow::Build(line, /* num_cols */ 1,
                    /* remove_whitespace */ true,
                    /* delimiter */ '-'),
      ResultIs(FailureExecutionResult(CSV_ROW_UNEXPECTED_NUMBER_OF_COLUMNS)));
}

TEST(CsvRowTest, GetColumnShouldFailIfOutOfBounds) {
  auto line = "val1,val2";
  ASSERT_SUCCESS_AND_ASSIGN(auto row,
                            CsvRow::Build(line, /* num_cols */ 2,
                                          /* remove_whitespace */ true,
                                          /* delimiter */ ','));

  EXPECT_THAT(row, RowHasExactColumns(vector<string>({"val1", "val2"})));
  EXPECT_THAT(row.GetColumn(2),
              ResultIs(FailureExecutionResult(CSV_COL_INDEX_OUT_OF_BOUNDS)));
}

TEST(CsvRowTest, BuildShouldSupportEmptyColumnsWhenLeading) {
  auto line = "     ,val1";
  ASSERT_SUCCESS_AND_ASSIGN(auto row,
                            CsvRow::Build(line, /* num_cols */ 2,
                                          /* remove_whitespace */ true,
                                          /* delimiter */ ','));

  EXPECT_THAT(row, RowHasExactColumns(vector<string>({"", "val1"})));

  auto line1 = ",val1";
  ASSERT_SUCCESS_AND_ASSIGN(auto row1,
                            CsvRow::Build(line1, /* num_cols */ 2,
                                          /* remove_whitespace */ true,
                                          /* delimiter */ ','));

  EXPECT_THAT(row1, RowHasExactColumns(vector<string>({"", "val1"})));
}

TEST(CsvRowTest, BuildShouldSupportEmptyColumnsWhenTrailing) {
  auto line = "val1,   ";
  ASSERT_SUCCESS_AND_ASSIGN(auto row,
                            CsvRow::Build(line, /* num_cols */ 2,
                                          /* remove_whitespace */ true,
                                          /* delimiter */ ','));

  EXPECT_THAT(row, RowHasExactColumns(vector<string>({"val1", ""})));

  auto line1 = "val1,";
  ASSERT_SUCCESS_AND_ASSIGN(auto row1,
                            CsvRow::Build(line1, /* num_cols */ 2,
                                          /* remove_whitespace */ true,
                                          /* delimiter */ ','));

  EXPECT_THAT(row1, RowHasExactColumns(vector<string>({"val1", ""})));
}

TEST(CsvRowTest, BuildShouldReturnEmptyRowWhenInputIsEmpty) {
  auto line = "";
  ASSERT_SUCCESS_AND_ASSIGN(auto row,
                            CsvRow::Build(line, /* num_cols */ 0,
                                          /* remove_whitespace */ true,
                                          /* delimiter */ ','));

  EXPECT_THAT(row, RowHasExactColumns(vector<string>({})));
}

TEST(CsvRowTest, BuildShouldFailIfEmptyInputDoesNotMatchNumberOfColumns) {
  auto line = "";
  EXPECT_THAT(
      CsvRow::Build(line, /* num_cols */ 1,
                    /* remove_whitespace */ true,
                    /* delimiter */ '-'),
      ResultIs(FailureExecutionResult(CSV_ROW_UNEXPECTED_NUMBER_OF_COLUMNS)));
}

TEST(CsvRowTest, BuildShouldParseEmptyRow) {
  auto line = " ";
  ASSERT_SUCCESS_AND_ASSIGN(auto row,
                            CsvRow::Build(line, /* num_cols */ 1,
                                          /* remove_whitespace */ true,
                                          /* delimiter */ ','));

  EXPECT_THAT(row, RowHasExactColumns(vector<string>({""})));
}

TEST(CsvRowTest, BuildShouldParseEmptyRows) {
  auto line = ",";
  ASSERT_SUCCESS_AND_ASSIGN(auto row,
                            CsvRow::Build(line, /* num_cols */ 2,
                                          /* remove_whitespace */ true,
                                          /* delimiter */ ','));

  EXPECT_THAT(row, RowHasExactColumns(vector<string>({"", ""})));

  auto line1 = " ,    ";
  ASSERT_SUCCESS_AND_ASSIGN(auto row1,
                            CsvRow::Build(line1, /* num_cols */ 2,
                                          /* remove_whitespace */ true,
                                          /* delimiter */ ','));

  EXPECT_THAT(row1, RowHasExactColumns(vector<string>({"", ""})));
}

}  // namespace google::pair::common::test
