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

REGISTER_COMPONENT_CODE(ID_ENCRYPTOR, 0x0103)

DEFINE_ERROR_CODE(ID_ENCRYPTOR_NOT_DONE_WITH_EXISTING_ENCRYPTION, ID_ENCRYPTOR,
                  0x0001,
                  "The ID encryptor must finish its current encryption before "
                  "beginning another.",
                  scp::core::errors::HttpStatusCode::UNKNOWN)

}  // namespace google::pair::publisher_list_generator::errors
