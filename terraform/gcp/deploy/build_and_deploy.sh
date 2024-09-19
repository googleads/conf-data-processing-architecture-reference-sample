#!/usr/bin/env bash
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


set -eu

# Prints usage instructions
print_usage() {
  echo "Usage:  ./terraform/gcp/deploy/build_and_deploy.sh --environment=<environment> [--worker_image=<worker_image>] [--action=<action>] [--auto_approve=<true/false>]"
  printf "\n\t--environment  - The operator environment to deploy.\n"
  printf "\t--worker_image - Artifact registry worker image. Default uses tfvars variable.\n"
  printf "\t--action       - Terraform action to take (apply|destroy). Default is apply.\n"
  printf "\t--auto_approve - Whether to auto-approve the Terraform action. Default is false.\n"
}

setup() {
  # Create temporary directory for operator
    OPER_TEMP="/tmp/$OPER_ENV/oper_tar"
    rm -rf "$OPER_TEMP"
    mkdir -p "$OPER_TEMP"

    # build tar
    bazel build @com_google_adm_cloud_scp//operator/terraform/gcp:cc_operator_tar --java_language_version=11

    # extract tar
    TAR_FILE="$(bazel info bazel-bin)/external/com_google_adm_cloud_scp/operator/terraform/gcp/cc_operator_tar.tgz"
    tar xzf "$TAR_FILE" -C "$OPER_TEMP"

    cp -r $(bazel info workspace)/terraform/gcp/environments/$OPER_ENV/ "$OPER_TEMP/environments_cc_operator_service/$OPER_ENV"

    OPER_ENV_PATH="$OPER_TEMP/environments_cc_operator_service/$OPER_ENV"
}

# deletes the temp directory
function cleanup {
  if [[ -n "${OPER_TEMP+x}" ]]; then
    rm -rf "$OPER_TEMP"
  fi
}

# register the cleanup function to be called on the EXIT signal
trap cleanup EXIT

if [ $# -eq 0 ]
  then
    printf "ERROR: Operator environment (environment) must be provided.\n"
    exit 1
fi

FUNCTION=$1
# parse arguments
for ARGUMENT in "$@"
  do
    case $ARGUMENT in
      --worker_image=*)
        WORKER_IMAGE=$(echo "$ARGUMENT" | cut -f2 -d=)
        ;;
      --environment=*)
        OPER_ENV=$(echo "$ARGUMENT" | cut -f2 -d=)
        ;;
      --action=*)
        input=$(echo "$ARGUMENT" | cut -f2 -d=)
        if [[ $input = "apply" || $input = "destroy" || $input = "plan" ]]
        then
          ACTION=$input
        else
          printf "ERROR: Input action must be one of (apply|destroy|plan).\n"
          exit 1
        fi
        ;;
      --auto_approve=*)
        AUTO_APPROVE=$(echo "$ARGUMENT" | cut -f2 -d=)
        ;;
      help)
        print_usage
        exit 1
        ;;
      *)
        printf "ERROR: invalid argument $ARGUMENT\n"
        print_usage
        exit 1
        ;;
    esac
done

if [[ -z ${OPER_ENV+x} ]]; then
  printf "ERROR: Operator environment (environment) must be provided.\n"
  exit 1
fi

# Build and extract operator tar and environment.
setup

PARAMETERS=''

if [[ -n ${WORKER_IMAGE+x} ]]; then
    PARAMETERS="${PARAMETERS} --worker_image=$WORKER_IMAGE "
fi

if [[ -n ${ACTION+x} ]]; then
    PARAMETERS="${PARAMETERS} --action=$ACTION "
fi

if [[ -n ${AUTO_APPROVE+x} ]]; then
    PARAMETERS="${PARAMETERS} --auto_approve=$AUTO_APPROVE "
fi

# Deploy operator environment with specified action.
$(bazel info workspace)/terraform/gcp/deploy/operator_deploy.sh --environment_path=$OPER_ENV_PATH $PARAMETERS
