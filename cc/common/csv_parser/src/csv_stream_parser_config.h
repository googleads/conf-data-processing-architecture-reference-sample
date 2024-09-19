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

#include <limits>
#include <string>

constexpr char kDefaultCsvRowDelimiter = ',';
constexpr char kDefaultCsvLineBreak = '\n';
// 500 MiB
constexpr size_t kMaxCsvStreamParserBufferedDataSizeBytes =
    1024 * 1024 * 500;
constexpr size_t kDefaultCsvStreamParserBufferedDataSizeBytes = 1024;

namespace google::pair::common {
/**
 * @brief Class used to provide init config values to the CSV stream parser.
 *
 */
class CsvStreamParserConfig {
 public:
  CsvStreamParserConfig() = delete;

  /**
   * @brief Construct a new Csv Stream Parser Config object
   *
   * @param num_cols The expected number of columns in the CSV file
   * @param remove_whitespace Whether to remove whitespace when parsing
   * @param delimiter The delimiter character to split a row by
   * @param line_break The line break character used to distinguish rows
   * @param max_buffered_data_size The maximum amount of data to buffer at a
   * time
   */
  CsvStreamParserConfig(size_t num_cols, bool remove_whitespace = true,
                        char delimiter = kDefaultCsvRowDelimiter,
                        char line_break = kDefaultCsvLineBreak,
                        size_t max_buffered_data_size =
                            kDefaultCsvStreamParserBufferedDataSizeBytes)
      : num_cols_(num_cols),
        remove_whitespace_(remove_whitespace),
        delimiter_(delimiter),
        line_break_(line_break),
        max_buffered_data_size_(std::min(
            max_buffered_data_size, kMaxCsvStreamParserBufferedDataSizeBytes)) {

  }

  size_t GetNumCols() const { return num_cols_; }

  bool GetRemoveWhitespace() const { return remove_whitespace_; }

  char GetDelimiter() const { return delimiter_; }

  char GetLineBreak() const { return line_break_; }

  size_t GetMaxBufferedDataSize() const { return max_buffered_data_size_; }

 private:
  size_t num_cols_;
  bool remove_whitespace_;
  char delimiter_;
  char line_break_;
  size_t max_buffered_data_size_;
};

}  // namespace google::pair::common
