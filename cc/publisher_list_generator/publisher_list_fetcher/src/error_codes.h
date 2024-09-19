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

namespace google::pair::publisher_list_generator::errors {

REGISTER_COMPONENT_CODE(PUBLISHER_LIST_FETCHER, 0x0102)

DEFINE_ERROR_CODE(PUBLISHER_LIST_FETCHER_ERROR_OPENING_FILE,
                  PUBLISHER_LIST_FETCHER, 0x0001,
                  "The requested file was unable to be opened.",
                  scp::core::errors::HttpStatusCode::UNKNOWN)

DEFINE_ERROR_CODE(PUBLISHER_LIST_FETCHER_ERROR_PARSING_DATA,
                  PUBLISHER_LIST_FETCHER, 0x0002,
                  "The acquired data was unable to be parsed.",
                  scp::core::errors::HttpStatusCode::UNKNOWN)

}  // namespace google::pair::publisher_list_generator::errors
