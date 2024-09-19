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

cc_test(
    name = "match_worker_test",
    srcs = [
        "match_worker_test.cc",
    ],
    deps = [
        "//cc/matcher/match_worker/src:match_worker_lib",
        "//cc/common/attestation/src:attestation_info_lib",
        "//cc/common/blob_streamer/mock:blob_streamer_mock",
        "@com_google_adm_cloud_scp//cc/core/test/utils:utils_lib",
        "@com_google_adm_cloud_scp//cc/public/core/interface:execution_result",
        "@com_google_adm_cloud_scp//cc/public/core/test/interface:execution_result_matchers",
        "@com_google_adm_cloud_scp//cc/public/cpio/interface/blob_storage_client",
        "@com_google_adm_cloud_scp//cc/public/cpio/mock/blob_storage_client:blob_storage_client_mock",
        "@com_google_googletest//:gtest_main",
    ],
)
