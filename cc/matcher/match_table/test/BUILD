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
    name = "match_table_hash_map_test",
    srcs = [
        "match_table_hash_map_test.cc",
    ],
    deps = [
        "//cc/matcher/match_table/src:match_table_lib",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_adm_cloud_scp//cc/public/core/interface:execution_result",
        "@com_google_adm_cloud_scp//cc/public/core/test/interface:execution_result_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
