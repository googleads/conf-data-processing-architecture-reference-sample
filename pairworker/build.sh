#!/bin/bash
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

# This file is copied from the SCP repo //cc/public/cpio/build_deps/shared/reproducible_build.sh
# TODO: Extract the file from the SCP repo cc operator tar during build.

#######################################
# Setup the container that will be used to run the build,
#######################################
setup_build_time_container() {
  local path_to_build_time_image_target="$1"
  local path_to_build_time_image="$2"

  bazel build $path_to_build_time_image_target
  if ! [ -f "$path_to_build_time_image" ]; then
    echo "image [$path_to_build_time_image] does not exist!"
    exit
  fi
  docker load < $path_to_build_time_image
  return $?
}

#######################################
# Run the given command within the build time container,
# using the given directory as the working directory.
# Make sure the container allows running the command as if it
# were running on the host, by using the host's resources and
# executing everything as the user running this function on the host.
#######################################
run_within_build_time_container() {
  local image="$1"
  local startup_directory="$2"
  local command_to_run="$3"
  cp /etc/passwd /tmp/passwd.docker
  grep $USER /tmp/passwd.docker || getent passwd | grep $USER >> /tmp/passwd.docker
  docker_gid=$(getent group docker | cut -d: -f3)
  docker run -i \
      --privileged \
      --network host \
      --ipc host \
      -u=$(id -u):$docker_gid \
      -e USER=$USER \
      -e HOME=$HOME \
      -v /tmp/passwd.docker:/etc/passwd:ro \
      -v "$HOME:$HOME" \
      -v "/tmpfs:/tmpfs" \
      -v /var/run/docker.sock:/var/run/docker.sock \
      -w "$startup_directory" \
      $image \
      "$command_to_run"
  return $?
}