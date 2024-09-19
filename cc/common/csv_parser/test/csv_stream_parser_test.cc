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

#include "cc/common/csv_parser/src/csv_stream_parser.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include "cc/common/csv_parser/src/csv_row.h"
#include "cc/common/csv_parser/src/csv_stream_parser_config.h"
#include "cc/common/csv_parser/src/error_codes.h"
#include "cc/public/core/test/interface/execution_result_matchers.h"
#include "core/test/utils/conditional_wait.h"

using google::pair::common::CsvRow;
using google::pair::common::CsvStreamParser;
using google::pair::common::CsvStreamParserConfig;
using google::pair::common::errors::CSV_STREAM_PARSER_BUFFER_AT_CAPACITY;
using google::scp::core::FailureExecutionResult;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using std::atomic;
using std::string;
using std::thread;
using std::vector;
using testing::ElementsAre;

namespace google::pair::common::test {

TEST(CsvStreamParserTest, ShouldBeAbleToAddALineChunk) {
  CsvStreamParserConfig config(/* num_cols */ 2);
  CsvStreamParser parser(config);

  EXPECT_SUCCESS(parser.AddCsvChunk("row,"));
}

TEST(CsvStreamParserTest, ShouldNotHaveRowIfNoneIsComplete) {
  CsvStreamParserConfig config(/* num_cols */ 2);
  CsvStreamParser parser(config);

  EXPECT_SUCCESS(parser.AddCsvChunk("row,"));

  EXPECT_FALSE(parser.HasRow());
}

TEST(CsvStreamParserTest, ShouldHaveRowIfOneIsComplete) {
  CsvStreamParserConfig config(/* num_cols */ 2);
  CsvStreamParser parser(config);

  EXPECT_SUCCESS(parser.AddCsvChunk("val1,val2\n"));

  EXPECT_TRUE(parser.HasRow());
}

TEST(CsvStreamParserTest, ShouldBeAbleToRetrieveRow) {
  CsvStreamParserConfig config(/* num_cols */ 2);
  CsvStreamParser parser(config);

  EXPECT_SUCCESS(parser.AddCsvChunk("val1,val2\n"));
  EXPECT_TRUE(parser.HasRow());

  auto row = parser.GetNextRow();

  EXPECT_SUCCESS(row);
  EXPECT_EQ(*row->GetColumn(0), "val1");
  EXPECT_EQ(*row->GetColumn(1), "val2");
}

TEST(CsvStreamParserTest, ShouldFailToAddChunkIfBufferIsAtCapacity) {
  CsvStreamParserConfig config(/* num_cols */ 2, /* remove_whitespace */ true,
                               /* delimiter */ ',',
                               /* line_break */ '\n',
                               /* max_buffered_data_size */ 10);
  CsvStreamParser parser(config);

  // This line takes the entire buffer
  EXPECT_SUCCESS(parser.AddCsvChunk("val1,val2\n"));

  // This line should fail to be added
  EXPECT_THAT(
      parser.AddCsvChunk("1"),
      ResultIs(RetryExecutionResult(CSV_STREAM_PARSER_BUFFER_AT_CAPACITY)));
}

TEST(CsvStreamParserTest, ShouldBeAbleToAddDataOnceInternalBufferClearsUp) {
  CsvStreamParserConfig config(/* num_cols */ 2, /* remove_whitespace */ true,
                               /* delimiter */ ',',
                               /* line_break */ '\n',
                               /* max_buffered_data_size */ 10);
  CsvStreamParser parser(config);

  // This row takes the entire buffer
  EXPECT_SUCCESS(parser.AddCsvChunk("val1,val2\n"));

  // This data should fail to be added
  EXPECT_THAT(
      parser.AddCsvChunk("1"),
      ResultIs(RetryExecutionResult(CSV_STREAM_PARSER_BUFFER_AT_CAPACITY)));

  EXPECT_TRUE(parser.HasRow());
  // This removes a row from the buffer and frees space for more data to be
  // added.
  EXPECT_SUCCESS(parser.GetNextRow());

  EXPECT_SUCCESS(parser.AddCsvChunk("1"));
}

TEST(CsvStreamParserTest, ShouldBeAbleToAddLineInMultipleChunks) {
  CsvStreamParserConfig config(/* num_cols */ 2);
  CsvStreamParser parser(config);

  EXPECT_SUCCESS(parser.AddCsvChunk("val1"));
  EXPECT_FALSE(parser.HasRow());

  EXPECT_SUCCESS(parser.AddCsvChunk(","));
  EXPECT_FALSE(parser.HasRow());

  EXPECT_SUCCESS(parser.AddCsvChunk("val2"));
  EXPECT_FALSE(parser.HasRow());

  EXPECT_SUCCESS(parser.AddCsvChunk("\n"));
  // We finally completed a line
  EXPECT_TRUE(parser.HasRow());

  auto row = parser.GetNextRow();
  EXPECT_SUCCESS(row);
  EXPECT_EQ(*row->GetColumn(0), "val1");
  EXPECT_EQ(*row->GetColumn(1), "val2");
}

TEST(CsvStreamParserTest, ShouldHoldLeftoverData) {
  CsvStreamParserConfig config(/* num_cols */ 3);
  CsvStreamParser parser(config);

  EXPECT_SUCCESS(parser.AddCsvChunk("val1"));
  EXPECT_SUCCESS(parser.AddCsvChunk(",val2 ,"));
  // This completes a row but also leaves more data in the buffer
  EXPECT_SUCCESS(parser.AddCsvChunk("val3 \nrow2-1, row2-2,row2-3"));

  EXPECT_TRUE(parser.HasRow());
  auto row = parser.GetNextRow();
  EXPECT_SUCCESS(row);
  EXPECT_EQ(*row->GetColumn(0), "val1");
  EXPECT_EQ(*row->GetColumn(1), "val2");
  EXPECT_EQ(*row->GetColumn(2), "val3");

  // Since we got that row, there should be no row ready
  EXPECT_FALSE(parser.HasRow());

  // This completes that row that we had leftover
  EXPECT_SUCCESS(parser.AddCsvChunk("\n"));
  EXPECT_TRUE(parser.HasRow());
  row = parser.GetNextRow();
  EXPECT_SUCCESS(row);
  EXPECT_EQ(*row->GetColumn(0), "row2-1");
  EXPECT_EQ(*row->GetColumn(1), "row2-2");
  EXPECT_EQ(*row->GetColumn(2), "row2-3");
}

TEST(CsvStreamParserTest, ShouldSubtractUsedBufferedDataWhenRowsAreRemoved) {
  CsvStreamParserConfig config(/* num_cols */ 2);
  CsvStreamParser parser(config);

  EXPECT_SUCCESS(parser.AddCsvChunk("val1,"));
  EXPECT_EQ(5, parser.GetBufferedDataSize());

  EXPECT_SUCCESS(parser.AddCsvChunk("val2\n"));
  EXPECT_EQ(10, parser.GetBufferedDataSize());

  EXPECT_SUCCESS(parser.GetNextRow());

  // There should be nothing left
  EXPECT_EQ(0, parser.GetBufferedDataSize());
}

static const vector<string> CsvRowsToStringRows(vector<CsvRow>& rows) {
  vector<string> output;
  for (auto& r : rows) {
    output.push_back("[" + *r.GetColumn(0) + "," + *r.GetColumn(1) + "]");
  }
  return output;
}

TEST(CsvStreamParserTest, ShouldSupportAddAndGetFromTwoDifferentThreads) {
  CsvStreamParserConfig config(/* num_cols */ 2, /* remove_whitespace */ true,
                               /* delimiter */ ',',
                               /* line_break */ '\n',
                               /* max_buffered_data_size */ 10);
  CsvStreamParser parser(config);

  // Since the cap for buffering is 10 bytes above, we set these up to always
  // complete at least a row within the buffer space.
  vector<string> input = {"val1,val2\n", "val",         "3",
                          ",",           "val4",        "\n",
                          "v1,2\n",      "val5,val6\n", "val7,val8\n"};
  vector<CsvRow> output;

  atomic<bool> done_pushing_data = false;
  atomic<bool> done_getting_data = false;

  thread add_data([&parser, &input, &done_pushing_data]() {
    for (auto& chunk : input) {
      auto result = SuccessExecutionResult();
      do {
        result = parser.AddCsvChunk(chunk);

        auto success = result.Successful();
        auto retry = result.Retryable();
        EXPECT_TRUE(success || retry)
            << "success: " << success << " retry: " << retry;
      } while (!result.Successful());
    }

    done_pushing_data.store(true);
  });

  thread get_data([&parser, &output, &done_pushing_data, &done_getting_data]() {
    while (!done_pushing_data.load() || parser.HasRow()) {
      if (parser.HasRow()) {
        auto row = parser.GetNextRow();
        output.push_back(*row);
      }
    }

    done_getting_data.store(true);
  });

  WaitUntil([&done_getting_data]() { return done_getting_data.load(); });

  auto output_lines = CsvRowsToStringRows(output);

  EXPECT_THAT(output_lines, ElementsAre("[val1,val2]", "[val3,val4]", "[v1,2]",
                                        "[val5,val6]", "[val7,val8]"));

  if (add_data.joinable()) {
    add_data.join();
  }
  if (get_data.joinable()) {
    get_data.join();
  }
}

}  // namespace google::pair::common::test
