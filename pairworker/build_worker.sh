#!/bin/bash
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

# Builds the PAIR worker in a docker container and uploads the image to artifact
# registry. The SCP repo is cloned locally and mounted since git-on-borg repos
# cannot be accessed from within the docker container.

set -euox pipefail

# Run the script from the root of the repo.
code_repo_dir=$(git rev-parse --show-toplevel)

source "${code_repo_dir}/pairworker/build.sh"

temp_scp_path="${code_repo_dir}/../pairbuild"

run_on_exit() {
  rm -rf temp_scp_path

  if [ "$1" == "0" ]; then
    echo "Done :)"
  else
    echo "Done :("
  fi

  exit 1
}

# Make sure run_on_exit runs even when we encounter errors
trap "run_on_exit 1" ERR

# Set up the temporary working directories
rm -rf "$temp_scp_path"
mkdir -p "$temp_scp_path"

# Determine the SCP tag to use for the repository in the container
scp_version=$(sed -rn 's/^SCP_VERSION\s*=\s*"(.*)"\s*(#.*)?$/\1/p' "${code_repo_dir}/WORKSPACE")
if [ -z "${scp_version}" ]; then
  echo "Failed to read the SCP tag from '${repo_top_level_dir}/WORKSPACE'."
  exit 1
fi

echo "SCP version: ${scp_version}"
# Fetch the internal SCP repository (required as a dependency)
pushd "${temp_scp_path}"
  # TODO change to git repo path
  (git clone "sso://team/adm-cloud-git-owners/scp" --depth 1 --branch ${scp_version})
popd

# Update the pair dependencies in the WORKSPACE file for a container build
sed -i 's/^#\s*CONTAINER_BUILD__UNCOMMENT://' "${code_repo_dir}/WORKSPACE"
sed -i -n '/^#\s*CONTAINER_BUILD__REMOVE_SECTION\.START/,/^#\s*CONTAINER_BUILD__REMOVE_SECTION\.END/!p' "${code_repo_dir}/WORKSPACE"

pushd "${code_repo_dir}"
  setup_build_time_container "pairworker:pair_build_time_image.tar" "bazel-bin/pairworker/pair_build_time_image.tar"
popd

build_command="bazel run --@com_google_adm_cloud_scp//cc/public/cpio/build_deps/shared:reproducible_build=False --@com_google_adm_cloud_scp//cc/public/cpio/interface:platform=gcp --@com_google_adm_cloud_scp//cc/public/cpio/interface:run_inside_tee=True //pairworker:pair_worker_dev --client_env=BAZEL_CXXOPTS=-std=c++17 --cxxopt=-std=c++17"

run_within_build_time_container "bazel/pairworker:pair_build_time_image" "$code_repo_dir" "$build_command"
