# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@io_bazel_rules_docker//container:container.bzl", "container_pull")

def non_opensource_deps():
    container_pull(
        name = "debian_11",
        digest = "sha256:640e07a7971e0c13eb14214421cf3d75407e0965b84430e08ec90c336537a2cf",
        registry = "index.docker.io",
        repository = "amd64/debian",
        tag = "11",
    )
