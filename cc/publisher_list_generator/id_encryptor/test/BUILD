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
    name = "random_id_encryptor_test",
    srcs = [
        "random_id_encryptor_test.cc",
    ],
    deps = [
        "//cc/publisher_list_generator/id_encryptor/src:id_encryptor_lib",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/random",
        "@com_google_adm_cloud_scp//cc/core/test/utils:utils_lib",
        "@com_google_adm_cloud_scp//cc/core/async_executor/mock:core_async_executor_mock",
        "@com_google_adm_cloud_scp//cc/core/interface:streaming_context_lib",
        "@com_google_adm_cloud_scp//cc/public/core/interface:execution_result",
        "@com_google_adm_cloud_scp//cc/public/core/test/interface:execution_result_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
