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

namespace google::pair::matcher::errors {

REGISTER_COMPONENT_CODE(MATCH_WORKER, 0x0102)

DEFINE_ERROR_CODE(
    MATCH_WORKER_FAILED_PARSING_PUBLISHER_MAPPING, MATCH_WORKER, 0x0001,
    "The publisher mapping acquired from storage could not be parsed.",
    scp::core::errors::HttpStatusCode::BAD_REQUEST)

}  // namespace google::pair::matcher::errors
