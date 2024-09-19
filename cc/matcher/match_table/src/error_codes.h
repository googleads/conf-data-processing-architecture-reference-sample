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

REGISTER_COMPONENT_CODE(MATCH_TABLE, 0x0101)

DEFINE_ERROR_CODE(MATCH_TABLE_ELEMENT_ALREADY_EXISTS, MATCH_TABLE, 0x0001,
                  "The element added to the match table already exists.",
                  scp::core::errors::HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(MATCH_TABLE_ELEMENT_DOES_NOT_EXIST, MATCH_TABLE, 0x0002,
                  "The element does not exist in the match table.",
                  scp::core::errors::HttpStatusCode::BAD_REQUEST)

}  // namespace google::pair::matcher::errors
