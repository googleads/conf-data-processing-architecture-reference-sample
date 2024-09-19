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


input_mapping_blob_path=$1

job_id=`uuidgen`

body='{"job_id":"'"$job_id"'",'
body="$body"'"job_body":"{'
body="$body"'\"jobType\":\"JOB_TYPE_MATCH\",'
body="$body"'\"publisherInputBucket\":\"pair-demo-publisher-input\",'
body="$body"'\"publisherMappingBlobPath\":\"'"$input_mapping_blob_path"'\",'
body="$body"'\"advertiserInputBucket\":\"pair-demo-advertiser-input\",'
body="$body"'\"advertiserUserListBlobPath\":\"advInputList.csv\",'
body="$body"'\"matchOutputBucket\":\"pair-demo-publisher-output\",'
body="$body"'\"matchListBlobPath\":\"matchList'$job_id'.csv\",'
body="$body"'\"publisherBucketAttestationInfo\":{'
# Change the below project_id and wip_provider to the correct ones for your project.
body="$body"'\"project_id\":\"atlantean-stone-429613-b1\",'
body="$body"'\"wip_provider\":\"projects/1060696143196/locations/global/workloadIdentityPools/pairdemopool/providers/oidcpairprovider\"'
body="$body"'},'  # end publisherBucketAttestationInfo
body="$body"'\"advertiserBucketAttestationInfo\": {'
# Change the below project_id and wip_provider to the correct ones for your project.
# NOTE: This can be different from the WIP details for the publisher bucket.
body="$body"'\"project_id\":\"atlantean-stone-429613-b1\",'
body="$body"'\"wip_provider\":\"projects/1060696143196/locations/global/workloadIdentityPools/pairdemopool/providers/oidcpairprovider\"'
body="$body"'}'  # end advertiserBucketAttestationInfo
body="$body"'}"'  # end job_body string
body="$body"'}'  # end body

# Replace the URL below with the URL output by the terraform apply step. Don't forget to append "/v1alpha/createJob"
curl -H "Authorization: Bearer $(gcloud auth print-identity-token)" \
  -X POST \
  -d "$body" \
  https://pair-dev-us-east1-frontend-service-7cpfoilvja-ue.a.run.app/v1alpha/createJob