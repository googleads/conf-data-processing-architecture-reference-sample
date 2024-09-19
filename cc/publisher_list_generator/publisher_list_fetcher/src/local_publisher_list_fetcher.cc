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

#include "local_publisher_list_fetcher.h"

#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "cc/common/csv_parser/src/csv_stream_parser.h"
#include "cc/core/common/global_logger/src/global_logger.h"
#include "cc/public/core/interface/execution_result.h"

#include "error_codes.h"

using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::common::kZeroUuid;
using std::getline;
using std::ifstream;
using std::ios_base;
using std::make_unique;
using std::move;
using std::strerror;
using std::string;

constexpr char kLocalPublisherListFetcher[] = "LocalPublisherListFetcher";
constexpr size_t kNumCsvColumns = 1;

namespace google::pair::publisher_list_generator {

LocalPublisherListFetcher::LocalPublisherListFetcher()
    : csv_parser_(make_unique<common::CsvStreamParser>(
          common::CsvStreamParserConfig(kNumCsvColumns))) {}

ExecutionResultOr<FetchIdsResponse>
LocalPublisherListFetcher::FetchPublisherIds(FetchIdsRequest request) {
  FetchIdsResponse resp;
  ifstream file(request.bucket_name, ios_base::in);
  if (file.fail()) {
    auto result = FailureExecutionResult(
        errors::PUBLISHER_LIST_FETCHER_ERROR_OPENING_FILE);
    SCP_ERROR(kLocalPublisherListFetcher, kZeroUuid, result,
              "Failed opening file %s with %s", request.bucket_name.c_str(),
              strerror(errno));
    return result;
  }
  string line;
  while (getline(file, line, ',')) {
    RETURN_IF_FAILURE(csv_parser_->AddCsvChunk(line));
  }
  if (file.fail() && !file.eof()) {
    auto result = FailureExecutionResult(
        errors::PUBLISHER_LIST_FETCHER_ERROR_PARSING_DATA);
    SCP_ERROR(kLocalPublisherListFetcher, kZeroUuid, result,
              "Failed parsing file %s with %s", request.bucket_name.c_str(),
              strerror(errno));
    return result;
  }
  auto row_or = csv_parser_->GetNextRow();
  while (row_or.Successful()) {
    ASSIGN_OR_RETURN(auto id, row_or->GetColumn(0));
    resp.ids.emplace_back(move(id));
    row_or = csv_parser_->GetNextRow();
  }
  return resp;
}

}  // namespace google::pair::publisher_list_generator
