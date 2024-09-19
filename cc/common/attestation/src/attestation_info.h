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

#include "cc/public/cpio/proto/common/v1/cloud_identity_info.pb.h"

namespace google::pair::common {

/**
 * @brief Constructs a CloudIdentityInfo with the provided project ID and WIP
 * provider for GCP.
 *
 * @param project_id
 * @param wip_provider
 * @return google::cmrt::sdk::common::v1::CloudIdentityInfo
 */
google::cmrt::sdk::common::v1::CloudIdentityInfo BuildGcpCloudIdentityInfo(
    std::string_view project_id, std::string_view wip_provider);

}  // namespace google::pair::common
