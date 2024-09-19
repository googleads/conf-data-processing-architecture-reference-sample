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

#include <sstream>
#include <string>
#include <vector>

#include "cc/public/core/interface/execution_result.h"

namespace google::pair::common {
/**
 * @brief Class representing a CSV row.
 *
 */
class CsvRow {
 public:
  /**
   * @brief Build a CSV row object. This method has some light parsing built it,
   * where it accepts what is expected to be an unparsed CSV row and it'll
   * validate and parse it.
   *
   * @param csv_row the unparsed CSV row
   * @param num_cols the expected number of columns in the CSV row
   * @param remove_whitespace whether to remove whitespace from the line when
   * parsing
   * @param delimiter the column value delimiter
   * @return scp::core::ExecutionResultOr<CsvRow>
   */
  static scp::core::ExecutionResultOr<CsvRow> Build(const std::string& csv_row,
                                                    size_t num_cols,
                                                    bool remove_whitespace,
                                                    char delimiter);

  /**
   * @brief Get a given column.
   *
   * @param index the index of the column
   * @return scp::core::ExecutionResultOr<std::string> returns a failure if the
   * index is out of bounds
   */
  scp::core::ExecutionResultOr<std::string> GetColumn(size_t index) const;

 private:
  CsvRow() = default;

  std::vector<std::string> columns_;
};

}  // namespace google::pair::common