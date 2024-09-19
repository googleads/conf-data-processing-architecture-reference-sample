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
    name = "generator_lib",
    hdrs = [
        "generator.h",
    ],
    deps = [
        "//cc/publisher_list_generator/id_encryptor/src:id_encryptor_lib",
        "//cc/publisher_list_generator/proto:publisher_pair_list_cc_proto",
        "//cc/publisher_list_generator/publisher_list_fetcher/src:publisher_list_fetcher_lib",
        "//cc/publisher_list_generator/publisher_mapping_uploader/src:publisher_mapping_uploader_lib",
        "@com_google_absl//absl/strings",
        "@com_google_adm_cloud_scp//cc/public/core/interface:execution_result",
        "@com_google_adm_cloud_scp//cc/public/cpio/interface/blob_storage_client",
        "@com_google_adm_cloud_scp//cc/public/cpio/utils/sync_utils/src:sync_utils",
    ],
)
