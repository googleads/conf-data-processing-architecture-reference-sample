# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "publisher_mapping_uploader_lib",
    srcs = [
        "gcs_publisher_mapping_uploader.cc",
    ],
    hdrs = [
        "gcs_publisher_mapping_uploader.h",
        "publisher_mapping_uploader.h",
    ],
    deps = [
        "//cc/common/attestation/src:attestation_info_lib",
        "//cc/publisher_list_generator/proto:publisher_pair_list_cc_proto",
        "@com_google_absl//absl/strings",
        "@com_google_adm_cloud_scp//cc/public/core/interface:execution_result",
        "@com_google_adm_cloud_scp//cc/public/cpio/interface/blob_storage_client",
    ],
)
