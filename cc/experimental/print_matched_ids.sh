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


pub_bucket_name=$1
pub_object_name=$2
adv_bucket_name=$3
adv_object_name=$4
output_bucket_name=$5
output_object_name=$6

echo 'PubMapping:'
gcloud storage cat gs://$pub_bucket_name/$pub_object_name
echo 'AdvList:'
gcloud storage cat gs://$adv_bucket_name/$adv_object_name
echo 'Matched Encrypted IDs'
gcloud storage cat gs://$output_bucket_name/$output_object_name