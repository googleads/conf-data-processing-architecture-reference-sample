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

load("@com_google_adm_cloud_scp//cc/public/cpio/build_deps/native_lib/gcp:gcp_sdk_lib_container.bzl", "gcp_sdk_lib_container")
load("@com_google_adm_cloud_scp//cc/public/cpio/build_deps/shared:build_time_image.bzl", "build_time_image")

package(default_visibility = ["//visibility:public"])

gcp_sdk_lib_container(
    name = "pair_worker_dev",
    client_binaries = {"//cc/worker_runner:worker_runner": [
        "/worker_runner",
    ]},
    // TODO fill these out.
    image_registry = "<image_registry from Artifact Registry setup>",
    image_repository = "<project_id/artifact_repository/image_name>",
    image_tag = "dev",
    inside_tee = "True",
)

build_time_image(
    name = "pair_build_time_image",
    additional_pkgs_to_install = ["patch"],
)
