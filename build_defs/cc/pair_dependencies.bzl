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

load("@com_google_adm_cloud_scp//build_defs/cc/shared:bazel_rules_cpp.bzl", "bazel_rules_cpp")
load("@com_google_adm_cloud_scp//build_defs/cc/shared:cc_utils.bzl", "cc_utils")
load("@com_google_adm_cloud_scp//build_defs/shared:absl.bzl", "absl")
load("@com_google_adm_cloud_scp//build_defs/shared:bazel_build_tools.bzl", "bazel_build_tools")
load("@com_google_adm_cloud_scp//build_defs/shared:bazel_docker_rules.bzl", "bazel_docker_rules")
load("@com_google_adm_cloud_scp//build_defs/shared:bazel_rules_pkg.bzl", "bazel_rules_pkg")
load("@com_google_adm_cloud_scp//build_defs/shared:golang.bzl", "go_deps")
load("@com_google_adm_cloud_scp//build_defs/shared:terraform.bzl", "terraform")
load("@com_google_adm_cloud_scp//build_defs/shared:java_grpc.bzl", "java_grpc")
load("@com_google_adm_cloud_scp//build_defs/cc/shared:boost.bzl", "boost")
load("@com_google_adm_cloud_scp//build_defs/cc/shared:google_cloud_cpp.bzl", "import_google_cloud_cpp")
load("@com_google_adm_cloud_scp//build_defs/cc/shared:nghttp2.bzl", "nghttp2")
load("@com_google_adm_cloud_scp//build_defs/shared:protobuf.bzl", "protobuf")


def pair_dependencies(PROTOBUF_CORE_VERSION, PROTOBUF_SHA_256):
    absl()
    bazel_build_tools()
    bazel_docker_rules()
    bazel_rules_cpp()
    bazel_rules_pkg()
    cc_utils()
    boost()
    go_deps()
    java_grpc()
    nghttp2()
    protobuf(PROTOBUF_CORE_VERSION, PROTOBUF_SHA_256)
    terraform()
    import_google_cloud_cpp()
