/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "cc/core/interface/errors.h"
#include "cc/public/core/interface/execution_result.h"

namespace google::pair::common::errors {

REGISTER_COMPONENT_CODE(CSV_ROW, 0x0601)

DEFINE_ERROR_CODE(
    CSV_ROW_UNEXPECTED_NUMBER_OF_COLUMNS, CSV_ROW, 0x0001,
    "There was an unexpected number of columns when parsing the row.",
    scp::core::errors::HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(CSV_COL_INDEX_OUT_OF_BOUNDS, CSV_ROW, 0x0002,
                  "Column index out of bounds.",
                  scp::core::errors::HttpStatusCode::BAD_REQUEST)

REGISTER_COMPONENT_CODE(CSV_STREAM_PARSER, 0x0602)

DEFINE_ERROR_CODE(CSV_STREAM_PARSER_BUFFER_AT_CAPACITY, CSV_STREAM_PARSER,
                  0x0001, "The stream parser buffer is at capacity.",
                  scp::core::errors::HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(CSV_STREAM_PARSER_NO_ROW_AVAILABLE, CSV_STREAM_PARSER, 0x0002,
                  "There are no rows available to get.",
                  scp::core::errors::HttpStatusCode::BAD_REQUEST)

}  // namespace google::pair::common::errors
