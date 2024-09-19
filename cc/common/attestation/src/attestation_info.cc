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

#include "attestation_info.h"

#include <string_view>

#include "cc/public/cpio/proto/common/v1/cloud_identity_info.pb.h"

using google::cmrt::sdk::common::v1::CloudIdentityInfo;
using std::string_view;

namespace google::pair::common {

CloudIdentityInfo BuildGcpCloudIdentityInfo(string_view project_id,
                                            string_view wip_provider) {
  CloudIdentityInfo info;
  info.set_owner_id(project_id);
  info.mutable_attestation_info()
      ->mutable_gcp_attestation_info()
      ->set_wip_provider(wip_provider);
  return info;
}

}  // namespace google::pair::common
