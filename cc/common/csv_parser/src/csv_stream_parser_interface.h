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

#include <string_view>

#include "cc/public/core/interface/execution_result.h"

#include "csv_row.h"

namespace google::pair::common {
/**
 * @brief This class is used to buffer CSV data chunks that come in the right
 * order. Pieces of the CSV file's content can be added to this parser and then,
 * as valid parsed rows become ready, they can be retrieved from the parser.
 * The expected use case is that at most one thread calls AddCsvChunk and at
 * most one thread calls HasLine and GetLine. These can be two separate threads.
 * The idea is that one thread adds chunks of data while another threads reads
 * the parsed lines from it.
 *
 */
class CsvStreamParserInterface {
 public:
  /**
   * @brief Add chunk of data that from CSV lines. This function is not thread
   * safe and is expected to be called by a single thread providing chunks in
   * order.
   *
   * @param chunk The chunk of data
   * @return Success when the data was added, Retry if the operation can be
   * retried or Failure if the current flow failed completely
   */
  virtual scp::core::ExecutionResult AddCsvChunk(
      std::string_view chunk) noexcept = 0;

  /**
   * @brief Whether the parser was able to build a complete CSV row and it's
   * available for consumption.
   *
   * @return true
   * @return false
   */
  virtual bool HasRow() const noexcept = 0;

  /**
   * @brief Get a row from the parser or a failure if no rows are available.
   *
   * @return scp::core::ExecutionResultOr<CsvRow>
   */
  virtual scp::core::ExecutionResultOr<CsvRow> GetNextRow() noexcept = 0;

  /**
   * @brief Get the current buffered data size.
   *
   * @return size_t
   */
  virtual size_t GetBufferedDataSize() const noexcept = 0;

  virtual ~CsvStreamParserInterface() = default;
};

}  // namespace google::pair::common
