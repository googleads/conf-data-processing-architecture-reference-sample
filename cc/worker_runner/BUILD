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

load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_proto_library")
load("@rules_proto//proto:defs.bzl", "proto_library")

package(default_visibility = ["//visibility:public"])

proto_library(
    name = "pair_job_data_proto",
    srcs = ["pair_job_data.proto"],
    deps = [],
)

cc_proto_library(
    name = "pair_job_data_cc_proto",
    deps = [":pair_job_data_proto"],
)

cc_binary(
    name = "worker_runner",
    srcs = [
        "worker_runner.cc",
    ],
    deps = [
        ":pair_job_data_cc_proto",
        "//cc/matcher/match_worker/src:match_worker_lib",
        "//cc/publisher_list_generator/generator/src:generator_lib",
        "@com_google_absl//absl/debugging:failure_signal_handler",
        "@com_google_adm_cloud_scp//cc/public/core/interface:errors",
        "@com_google_adm_cloud_scp//cc/public/cpio/interface:cpio",
        "@com_google_adm_cloud_scp//cc/public/cpio/interface/blob_storage_client",
        "@com_google_adm_cloud_scp//cc/public/cpio/interface/job_client",
        "@com_google_adm_cloud_scp//cc/public/cpio/interface/job_client:type_def",
        "@com_google_adm_cloud_scp//cc/public/cpio/utils/configuration_fetcher/interface:configuration_fetcher_interface",
        "@com_google_adm_cloud_scp//cc/public/cpio/utils/configuration_fetcher/src:configuration_fetcher",
        "@com_google_adm_cloud_scp//cc/public/cpio/utils/job_lifecycle_helper/src:job_lifecycle_helper",
        "@com_google_protobuf//:protobuf",
    ],
)
