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
    name = "publisher_list_fetcher_lib",
    srcs = [
        "gcs_publisher_list_fetcher.cc",
        "local_publisher_list_fetcher.cc",
    ],
    hdrs = [
        "error_codes.h",
        "gcs_publisher_list_fetcher.h",
        "local_publisher_list_fetcher.h",
        "publisher_list_fetcher.h",
    ],
    deps = [
        "//cc/common/csv_parser/src:csv_stream_parser_lib",
        "@com_google_adm_cloud_scp//cc/core/common/global_logger/src:global_logger_lib",
        "@com_google_adm_cloud_scp//cc/core/common/uuid/src:uuid_lib",
        "@com_google_adm_cloud_scp//cc/public/core/interface:execution_result",
        "@com_google_adm_cloud_scp//cc/public/cpio/interface/blob_storage_client",
        "@com_google_adm_cloud_scp//cc/public/cpio/proto/common/v1:common_cc_proto",
    ],
)
