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

#include "cc/common/csv_parser/src/csv_stream_parser_config.h"

#include <gtest/gtest.h>

namespace google::pair::common::test {

TEST(CsvStreamParserConfigTest, ShouldSetNumCols) {
  CsvStreamParserConfig config(/* num_cols */ 1);

  EXPECT_EQ(1, config.GetNumCols());
}

TEST(CsvStreamParserConfigTest, ShouldSetRemoveWhitespaceToTrueByDefault) {
  CsvStreamParserConfig config(/* num_cols */ 1);

  EXPECT_TRUE(config.GetRemoveWhitespace());
}

TEST(CsvStreamParserConfigTest, ShouldSetRemoveWhitespaceBasedOnConstructor) {
  CsvStreamParserConfig config(/* num_cols */ 1, /* remove_whitespace */ false);

  EXPECT_FALSE(config.GetRemoveWhitespace());
}

TEST(CsvStreamParserConfigTest, ShouldSetDelimitedToDefault) {
  CsvStreamParserConfig config(/* num_cols */ 1);

  EXPECT_EQ(kDefaultCsvRowDelimiter, config.GetDelimiter());
}

TEST(CsvStreamParserConfigTest, ShouldSetDelimitedBasedOnConstructor) {
  CsvStreamParserConfig config(/* num_cols */ 1, /* remove_whitespace */ true,
                               /* delimiter */ '-');

  EXPECT_EQ('-', config.GetDelimiter());
}

TEST(CsvStreamParserConfigTest, ShouldSetLineBreakByDefault) {
  CsvStreamParserConfig config(/* num_cols */ 1);

  EXPECT_EQ(kDefaultCsvLineBreak, config.GetLineBreak());
}

TEST(CsvStreamParserConfigTest, ShouldSetLineBreakBasedOnConstructor) {
  CsvStreamParserConfig config(/* num_cols */ 1, /* remove_whitespace */ true,
                               /* delimiter */ ',', /* line_break */ '|');

  EXPECT_EQ('|', config.GetLineBreak());
}

TEST(CsvStreamParserConfigTest, ShouldSetMaxBufferedDataSizeByDefault) {
  CsvStreamParserConfig config(/* num_cols */ 1);

  EXPECT_EQ(kDefaultCsvStreamParserBufferedDataSizeBytes,
            config.GetMaxBufferedDataSize());
}

TEST(CsvStreamParserConfigTest, ShouldSetMaxBufferedDataSizBasedOnConstructor) {
  CsvStreamParserConfig config(/* num_cols */ 1, /* remove_whitespace */ true,
                               /* delimiter */ ',', /* line_break */ '\n',
                               /* max_buffered_data_size */ 123);

  EXPECT_EQ(123, config.GetMaxBufferedDataSize());
}

TEST(CsvStreamParserConfigTest,
     ShouldCapMaxBufferedDataSizeIfLargerThanExpected) {
  CsvStreamParserConfig config(
      /* num_cols */ 1, /* remove_whitespace */ true,
      /* delimiter */ ',', /* line_break */ '\n',
      /* max_buffered_data_size */ kMaxCsvStreamParserBufferedDataSizeBytes +
          1);

  EXPECT_EQ(kMaxCsvStreamParserBufferedDataSizeBytes,
            config.GetMaxBufferedDataSize());
}

}  // namespace google::pair::common::test
