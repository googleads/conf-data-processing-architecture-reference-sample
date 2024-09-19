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

#include "csv_stream_parser.h"

#include <mutex>
#include <sstream>
#include <string_view>

#include "error_codes.h"

using google::pair::common::errors::CSV_STREAM_PARSER_BUFFER_AT_CAPACITY;
using google::pair::common::errors::CSV_STREAM_PARSER_NO_ROW_AVAILABLE;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::ConcurrentQueue;
using std::getline;
using std::ios;
using std::make_unique;
using std::string;
using std::string_view;
using std::stringstream;

// Very large number since we don't expect the insertion into the
// concurrent queue to fail and it is treated as an error.
static constexpr size_t kCsvStreamParserConcurrentQueueCapacity = 100000000;

namespace google::pair::common {

CsvStreamParser::CsvStreamParser(const CsvStreamParserConfig& config)
    : config_(config),
      rows_(make_unique<ConcurrentQueue<string>>(
          kCsvStreamParserConcurrentQueueCapacity)),
      buffered_data_size_(0) {}

ExecutionResult CsvStreamParser::AddCsvChunk(string_view chunk) noexcept {
  // This means we've reached the limit of how much data we're willing to
  // buffer.
  if (chunk.size() + buffered_data_size_.load() >
      config_.GetMaxBufferedDataSize()) {
    return RetryExecutionResult(CSV_STREAM_PARSER_BUFFER_AT_CAPACITY);
  }

  buffered_data_size_ += chunk.size();

  stringstream current_data = move(rolling_data_);
  current_data << chunk;
  string line;
  auto rolling_cursor = current_data.tellg();

  while (getline(current_data, line, config_.GetLineBreak())) {
    // We reached the end of the current stream so we need to check whether the
    // last character was a line break or not.
    if (current_data.eof()) {
      // Move the cursor to the end to get the stream size.
      current_data.seekg(0, ios::end);
      auto last_char_index = static_cast<int>(current_data.tellg()) - 1;
      // Move the cursor to the index we care about.
      current_data.seekg(last_char_index, ios::beg);

      // This means that we got to the end of the stream and didn't really find
      // a line break so we don't even have a full line buffered.
      if (static_cast<char>(current_data.get()) != config_.GetLineBreak()) {
        // In this case we just return the cursor back to the beginning of this
        // sequence and break out of this loop.
        current_data.seekg(0, ios::beg);
        current_data.seekg(rolling_cursor, ios::cur);
        break;
      } else {
        // Even though we reached the end of the stream, we did find a full
        // row.
        // If this fails, it is unexpected and it is an error condition.
        RETURN_IF_FAILURE(rows_->TryEnqueue(line));
      }
    } else {
      // We found a full row.
      // If this fails, it is unexpected and it is an error condition.
      RETURN_IF_FAILURE(rows_->TryEnqueue(line));
      rolling_cursor = current_data.tellg();
    }
  }

  rolling_data_.clear();

  // The current_data stream will be marked as failed if getline could not get
  // any data out of it, in which case we don't want to access it.
  if (!current_data.fail()) {
    rolling_data_ << current_data.rdbuf();
  }

  return SuccessExecutionResult();
}

bool CsvStreamParser::HasRow() const noexcept {
  return rows_->Size() > 0;
}

ExecutionResultOr<CsvRow> CsvStreamParser::GetNextRow() noexcept {
  if (!HasRow()) {
    return FailureExecutionResult(CSV_STREAM_PARSER_NO_ROW_AVAILABLE);
  }

  string row;
  RETURN_IF_FAILURE(rows_->TryDequeue(row));

  // We add one to account for the line break char
  buffered_data_size_ -= row.size() + 1;

  return CsvRow::Build(row, config_.GetNumCols(), config_.GetRemoveWhitespace(),
                       config_.GetDelimiter());
}

size_t CsvStreamParser::GetBufferedDataSize() const noexcept {
  return buffered_data_size_.load();
}

}  // namespace google::pair::common
