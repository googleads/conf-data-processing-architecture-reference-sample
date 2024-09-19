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

#include "csv_row.h"

#include <string>

#include "absl/strings/ascii.h"
#include "cc/public/core/interface/execution_result.h"

#include "error_codes.h"

using absl::RemoveExtraAsciiWhitespace;
using google::pair::common::errors::CSV_COL_INDEX_OUT_OF_BOUNDS;
using google::pair::common::errors::CSV_ROW_UNEXPECTED_NUMBER_OF_COLUMNS;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using std::getline;
using std::istringstream;
using std::noskipws;
using std::string;
using std::stringstream;

namespace google::pair::common {

ExecutionResultOr<CsvRow> CsvRow::Build(const string& csv_row, size_t num_cols,
                                        bool remove_whitespace,
                                        char delimiter) {
  CsvRow ret;
  // If the input is empty just return an empty row
  if (csv_row.size() == 0 && num_cols == 0) {
    return ret;
  }
  if (csv_row.size() == 0 && num_cols != 0) {
    return FailureExecutionResult(CSV_ROW_UNEXPECTED_NUMBER_OF_COLUMNS);
  }

  string col;
  stringstream csv_row_stream;
  csv_row_stream << csv_row;
  csv_row_stream >> noskipws;
  ret.columns_.reserve(num_cols);
  // Account for the last character being a delimiter, meaning we want an empty
  // column.
  if (csv_row.at(csv_row.length() - 1) == delimiter) {
    csv_row_stream << " ";
  }

  while (getline(csv_row_stream, col, delimiter)) {
    if (remove_whitespace) {
      RemoveExtraAsciiWhitespace(&col);
    }
    ret.columns_.emplace_back(move(col));
  }

  if (ret.columns_.size() != num_cols) {
    return FailureExecutionResult(CSV_ROW_UNEXPECTED_NUMBER_OF_COLUMNS);
  }

  return ret;
}

ExecutionResultOr<string> CsvRow::GetColumn(size_t index) const {
  if (index >= columns_.size()) {
    return FailureExecutionResult(CSV_COL_INDEX_OUT_OF_BOUNDS);
  }

  return columns_.at(index);
}
}  // namespace google::pair::common