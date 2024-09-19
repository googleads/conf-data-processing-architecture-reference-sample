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

#include <atomic>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>

#include "cc/core/common/concurrent_queue/src/concurrent_queue.h"
#include "cc/public/core/interface/execution_result.h"

#include "csv_row.h"
#include "csv_stream_parser_config.h"
#include "csv_stream_parser_interface.h"

namespace google::pair::common {

class CsvStreamParser : public CsvStreamParserInterface {
 public:
  CsvStreamParser(const CsvStreamParserConfig& config);

  scp::core::ExecutionResult AddCsvChunk(
      std::string_view chunk) noexcept override;

  bool HasRow() const noexcept override;

  scp::core::ExecutionResultOr<CsvRow> GetNextRow() noexcept override;

  size_t GetBufferedDataSize() const noexcept override;

 private:
  /**
   * @brief The config object that the parser was initialized with.
   *
   */
  const CsvStreamParserConfig config_;

  /**
   * @brief Holds the rows that have been parsed so far.
   *
   */
  std::unique_ptr<scp::core::common::ConcurrentQueue<std::string>> rows_;

  /**
   * @brief Buffer containing the data that has been added to the parser so far.
   *
   */
  std::stringstream rolling_data_;

  /**
   * @brief This is a best effort accumulator to keep an upper limit on how much
   * data has been buffered.
   *
   */
  std::atomic<size_t> buffered_data_size_;
};

}  // namespace google::pair::common
