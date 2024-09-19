# Objective

Walk through how to integrate with the SCP codebase in order to launch a program in a TEE enabled Virtual Machine.

# Prerequisites

Have a GCP project you wish to run this demo on.

This GCP Project should have the following [APIs enabled](https://console.cloud.google.com/apis/dashboard):
* Artifact Registry API
* Cloud Spanner API
* Secret Manager API
* Cloud Functions API
* Cloud Build API
* Cloud Run Admin API
* Cloud Scheduler API
* Confidential Computing API

Be sure to have the following installed:

* [Bazel](https://bazel.build/install) The SCP repo uses Bazel to build and test code.
* [Terraform](https://developer.hashicorp.com/terraform/tutorials/aws-get-started/install-cli) Terraform is used to setup and spawn the necessary cloud platform (GCP) resources for running the SCP services
* [Python 3](https://www.python.org/downloads/)
* [gcloud CLI](https://cloud.google.com/sdk/docs/install)
  * Initialize gcloud following [this page](https://cloud.google.com/sdk/docs/initializing)
  * Don't forget to configure docker `gcloud auth configure-docker us-docker.pkg.dev`
* An IDE (IntelliJ for Java, VSCode for C++, etc.)

# Integrating with SCP

## Including the SCP Git Repository

Add the SCP git repository in the WORKSPACE file

```
SCP_VERSION = "v0.154.0"

git_repository(
    name = "com_google_adm_cloud_scp",
    remote = "TODO add url for the SCP Github",
    tag = SCP_VERSION,
)
```

## The Dataplane Worker

The Dataplane Worker is the binary that has a main function. This binary will use the CPIO libraries to interact with the cloud platform and provides an abstraction allowing one interface for multiple cloud platforms (currently GCP and AWS).

### CPIO Libraries

The CPIO clients will be the unified interface for executing operations on different cloud platforms. For example, the BlobStorageClient interface can be used to execute operations on either S3 (for AWS) or Cloud Storage (for GCP).

### Worker Runner

An example Worker Runner following the steps below can be found in `cc/worker_runner/worker_runner.cc`

To get started with your dataplane implementation, you can start out with setting these clients up for polling and completing jobs.

1. Initialize 2 AsyncExecutors. (One for CPU operations and one for IO)
1. Initialize ConfigurationFetcher (to fetch parameters set by the terraform operator environment such as job db, job queue, etc.)
1. Initalize CPIO
1. Initialize an AutoScalingClient and MetricInstanceFactory
1. Initialize a JobClient and a JobLifecycleHelper
   The JobLifecycleHelper is what will be used to fetch jobs
1. Creating a job polling loop
   The Job Polling loop is how the Worker Runner will poll for jobs, process jobs, and mark the jobs as complete.

### Worker Image Creation

To create a docker image with the above worker, follow the steps below:

**NOTE: Pushing to docker requires authentication which will use gcloud. To configure this, edit your [~/.docker/config.json](https://docs.docker.com/reference/cli/docker/login/#credential-helpers)**

Example:
```
{
  "credHelpers": {
    "us-east1-docker.pkg.dev": "gcloud"
  }
}
```

1. Create an [Artifact Registry](https://cloud.google.com/artifact-registry/docs/repositories/create-repos) for your worker images
1. Create a [`cc_binary`](https://bazel.build/reference/be/c-cpp#cc_binary) build target for your code
   Example: `cc/worker_runner:worker_runner` build target
1. Create a `gcp_sdk_lib_container` (TODO link to this build rule) build target to build/push your cc binary container to artifact registry
   Example: `pairworker:pair_worker_dev` container build target
1. Extract reproducible build script from SCP `cc_operator_tar` to create and build in build time container
1. Create a script to use the build time container to build and upload worker image

This script should (example including step 4 above: `pairworker/build_worker.sh`):
1. Clone the SCP repo locally to use in the build container since the SSO is not accessible from within the build container to clone the repo
1. Redirect workspace SCP dependency to the local temporary SCP repo
1. Set up the build time container
1. Run `run_within_build_time_container` using your build time container and your build command (builds and uploads image to Artifact Registry)

## Operator Infrastructure

The SCP Operator is a set of services useful to TEEs such as a Parameter Storage, Storage Buckets, and NoSQL tables. The cloud infrastructure for SCP is deployed and managed through [Terraform](https://www.terraform.io/).

### Create an Operator Environment

For each environment, create a blank service account to be used to run the compute engine worker instances. This will be referenced as the worker service account in the rest of this document. **Each environment needs its own worker service account** since the operator Terraform will assign some roles/permissions to the service account and could conflict if used across different environments.

The convenience script `terraform/gcp/deploy/build_and_deploy.sh` which does all of the following steps listed to build the worker and deploy the resources on GCP.

In your repo workspace, you should build the `cc_operator_tar`, which contains the operator terraform, and extract it.

```
# build tar
bazel build @com_google_adm_cloud_scp//operator/terraform/gcp:cc_operator_tar --java_language_version=11

# extract tar
TAR_FILE="$(bazel info bazel-bin)/external/com_google_adm_cloud_scp/operator/terraform/gcp/cc_operator_tar.tgz"
```

Copy the `demo` environment files into your new environment while maintaining the symlinks.

You should edit the details in the main.tf file in your environment. A non-exhaustive list of edits to make:

* Edit the backend “gcs” fields with a GCS bucket in your own project and path to store the terraform state for the environment. (the file location should NOT be shared among different environments otherwise Terraform states could be overwritten and there could be untracked resources.)
  * You may need to create a bucket if you do not have one already

You should edit the following in the `example.auto.tfvars` in your environment (rename it to `<environment>.auto.tfvars`):

* The `environment` variable should be unique and not shared with other environments since resources are named based on environment name and could conflict
* Fill in the `project_id`, `region`, and `region-zone` variable with your project's details
* `spanner_instance_config` should be a configuration best representing your region found [here](https://cloud.google.com/spanner/docs/instance-configurations?_gl=1*1vjrtyd*_ga*MTc4NjEwMDY0MS4xNzE3MTY1NDUw*_ga_WH2QY8WWF5*MTcyMTEzODI0Mi4yMy4xLjE3MjExMzk0MTQuNDYuMC4w#configs-multi-region). For example `regional-us-east1`
* `worker_image` should be the full path to the worker image. Something like `us-docker.pkg.dev/projectName/repoName/pair_worker_gcp:dev`
* Fill in the `user_provided_worker_sa_email` with the worker service account
  * This should be a service account in this GCP project. The terraform config will give it the proper permissions.

The rest of the variable options can be found in the variables.tf file

### Terraform Outputs

When terraform apply is run, the following will be output and should be recorded:

* `frontend_service_cloudfunction_url` - this Cloud Function url will be the entry point for your operator job requests (you should note this down to create jobs and get job info)
* `worker_service_account_email` - should be the same worker service account that you created and provided earlier
* `vpc_network` - the vpc network that is used to control network access to your operator environment resources

### Terraform Build/Deploy Scripts

After setting up your environment, an example script to build the `cc_operator_tar` to a temporary directory and copy over your environment to deploy can be found at `terraform/gcp/deploy/build_and_deploy.sh`. For this to work your environment directory name must match the environment name.

Example usage:
`./terraform/gcp/deploy/build_and_deploy.sh --environment="<environment_name>"`

## Attestation with Confidential Spaces

[Attestation is used in Confidential Spaces](https://cloud.google.com/docs/security/confidential-space) to ensure that resources/data are accessed from a specified image as well as allowing other configuration elements to be required such as the service account of the principal, etc.

### Resource Owner Responsibility

As a resource owner with resources that you only want to have accessed from within a TEE, you must create or configure a workload identity pool and associate the principal set to a resource or service account.

### Workload Identity Pool Creation

Create a [workload identity pool](https://cloud.google.com/iam/docs/manage-workload-identity-pools-providers) and provider with the configurations in this section. This is a reference to the workload identity pool policy attestation assertions for Confidential Spaces [Attestation policies | Confidential VM | Google Cloud](https://cloud.google.com/confidential-computing/confidential-vm/docs/reference/cs-attestation-policies)

#### Workload Identity Pool Provider Configurations

* Configure workload identity pool provider for confidential spaces attestation service (the provider name can be an arbitrary name)
  * Issuer (URL): https://confidentialcomputing.googleapis.com/
  * Allowed Audiences: https://sts.googleapis.com
* Required attribute conditions
  * assertion.swname - Used to verify the software running on the attested entity (This should always be set to CONFIDENTIAL_SPACES)
* Recommended attribute conditions
  * `google.subject` should be set as `assertion.sub`
  * assertion.submods.confidential_space.support_attributes - Used the verify the security version of the TEE in a Confidential Spaces Image
    * Leave empty if you want to allow using the confidential spaces debug image
    * USABLE: An image with only this attribute is out of support and no longer monitored for vulnerabilities. Use at your own risk.
    * STABLE: This version of the image is supported and monitored for vulnerabilities. A STABLE image is also USABLE.
    * LATEST: This is the latest version of the image, and is supported. The LATEST image is also STABLE and USABLE.
  * assertion.google_service_accounts - Used to verify that a specified service account is attached to the VM running the workload, or has been listed using tee-impersonate-service-account in the VM metadata. These should be the worker service accounts used in the operator environments.
* Worker Image Attribute Conditions
  * Choose whether to allowlist TEE worker images by image digest(hash), image reference(tag), or image signature. Can also be some combination.
  * assertion.submods.container.image_reference - Used to verify by image tag with format <region>/<project>/<artifact-registry-repo>/<image-name>:tag
  * assertion.submods.container.image_digest - Used to verify by image sha256 hash
  * assertion.submods.container.image_signatures - Used to verify by image signature
* Example attribute conditions for image tag
```
assertion.swname == 'CONFIDENTIAL_SPACE'
&& [].all(a, a in assertion.submods.confidential_space.support_attributes)
&& '<worker service account email>' in assertion.google_service_accounts
&& assertion.submods.container.image_reference in ['<artifact-registry-image:tag']
```

### Associate Workload Identity Pool 

On your resource permissions (in the Testing E2E example this will be the 3 storage buckets used), you can give access to the resource to the principal set of the workload identity pool provider.

Example principal set:
`principalSet://iam.googleapis.com/projects/<project_number>/locations/global/workloadIdentityPools/<workload_identity_pool_name>/*`

### Onboarding Operators to Access Resources

To allow an operator attested access to a resource, the operator must provide the resource owner the worker service account for their operator environment. Depending on who is developing the worker image, someone will provide the worker image information to the resource owner also. The worker service account and worker image will need to be added to the Workload Identity Pool Provider policy to be able to access the resource from the operator TEE.
The Artifact Registry Reader role must be applied to the worker service account(s).

## Operator Responsibility

### Accessing attested resources using CPIO libraries

For attested access to resources, the resource owner must provide the resource information/name, project id, and the workload identity pool provider to the operator. Attested access to resources is currently supported for blob storage client (GCS) and key service.

For example, blob storage client takes in a cloud_identity_info which are configurations for project id and workload identity pool provider.

# Testing E2E

Create 3 buckets for the Advertiser input, Publisher input, and Publisher output buckets.

Initialize 1 env using the Terraform script which starts one worker.

Create 2 workload identity pools, 1 for publisher and 1 for advertiser and grant the Publisher WIP access to both of the Publisher's buckets and the Advertiser WIP access to the Advertiser's bucket.

Follow the further steps below to upload files and run the jobs.

Using the example worker runner given above and launching it using the above infrastructure steps, we can post jobs to it and observe that they complete successfully.

First we can observe the worker runner’s Serial Port logs to ensure that it is polling for jobs.

```
...
cc/worker_runner/worker_runner.cc:main:408|4: PrepareNextJob didn't succeed Failed with: No messages are being received from PubSub
cc/worker_runner/worker_runner.cc:main:400|32: Polling for job.
...
```

This worker runner supports 2 types of jobs: 1. Publisher List Generation and 2. Publisher Advertiser Matching. Both jobs are represented using the same proto (found in cc/worker_runner/pair_job_data.proto:PairJobData) but with different fields set.

The scenario for this worker is that there are 2 actors - a publisher and an advertiser. Both the publisher and advertiser have a list (CSV) of emails and the publisher would like the advertiser to receive a list of only the PII that exists in both sets, without the advertiser being exposed to any unknown PII (aka a "confidential match"). We assume that the publisher has their list stored in Google Cloud Storage in some bucket (pair-demo-publisher-input/pubInputList.csv) as does the advertiser (pair-demo-advertiser-input/advInputList.csv). The publisher also has a plaintext file containing simply the name of the bucket they want to output the matched PIIs to (pair-demo-publisher-input/outputBucketName.txt). These buckets can be in different projects.

`cc/experimental/random_emails_generator.cc` can be used to generate a list of random emails to a file (say N = 20) via mode 1. Then in mode 2, it can be used to take a sample of these N emails (say M = 12) and create the publisher’s input list and the advertiser’s input list both in their respective GCS buckets.

Make a plaintext file with the output bucket name (the contents should be the name of the bucket the publisher mapping should be placed in; here we simply place the output back in the input bucket):
```
echo -n 'pair-demo-publisher-output' | gcloud storage cp - gs://pair-demo-publisher-input/outputBucketName.txt
```

Generate random emails in random_emails.csv:
```
bazel-bin/cc/experimental/random_emails_generator \
  1 random_emails.csv 20
```

Compose publisher input list:
```
bazel-bin/cc/experimental/random_emails_generator \
  2 random_emails.csv 12 pair-demo-publisher-input pubInputList.csv
```

Compose advertiser input list:
```
bazel-bin/cc/experimental/random_emails_generator \
  2 random_emails.csv 12 pair-demo-advertiser-input advInputList.csv
```

To obtain the list of matched PIIs, first we need to create a Publisher List Generation job which will create a mapping of the publisher’s PIIs to an encrypted/hashed version of that PII. To create this job, we can make a curl request to the SCP frontend service that was spun up as a part of the Operator Infrastructure. An example request may look like this `cc/experimental/create_list_gen_job.sh` suggests.

We can check the serial port of the worker VM to observe that it completed the job successfully and the generated list exists in pair-demo-publisher-input/pubMapping1234.csv.

```
cc/worker_runner/worker_runner.cc:main:417|32: Received a job: 82b450d1-e5b4-416a-8f3e-c44bddca0cd6
cc/worker_runner/worker_runner.cc:main:428|32: Parsed body: job_type: JOB_TYPE_GENERATE_PUB_PAIR_LIST
publisher_input_bucket: "pair-demo-publisher-input"
publisher_metadata_blob_path: "outputBucketName.txt"
publisher_user_list_blob_path: "pubInputList.csv"
publisher_mapping_blob_path: "pubMapping82b450d1-e5b4-416a-8f3e-c44bddca0cd6.csv"
publisher_bucket_attestation_info {
  project_id: "atlantean-stone-429613-b1"
  wip_provider: "projects/1060696143196/locations/global/workloadIdentityPools/pairdemopool/providers/oidcpairprovider"
}

cc/worker_runner/worker_runner.cc:main:432|32: Processing publisher list generation job.
cc/worker_runner/worker_runner.cc:main:443|32: Successfully generated publisher mapping to pubMapping82b450d1-e5b4-416a-8f3e-c44bddca0cd6.csv!
```

After this job completes, we need to create a job to perform the matching of both the publisher’s generated mapping (pair-demo-publisher-input/pubMapping1234.csv) and the advertiser’s input list (pair-demo-advertiser-input/advInputList.csv). The job can be created similarly to the previous job but with different fields. `cc/experimental/create_match_job.sh` illustrates how this can be done.

```
cc/worker_runner/worker_runner.cc:main:417|32: Received a job: 859fa370-a8d5-4fbe-a19a-4880ee130361
cc/worker_runner/worker_runner.cc:main:428|32: Parsed body: job_type: JOB_TYPE_MATCH
publisher_input_bucket: "pair-demo-publisher-input"
publisher_mapping_blob_path: "pubMapping82b450d1-e5b4-416a-8f3e-c44bddca0cd6.csv"
advertiser_input_bucket: "pair-demo-advertiser-input"
advertiser_user_list_blob_path: "advInputList.csv"
match_output_bucket: "pair-demo-publisher-output"
match_list_blob_path: "matchList859fa370-a8d5-4fbe-a19a-4880ee130361.csv"
publisher_bucket_attestation_info {
  project_id: "atlantean-stone-429613-b1"
  wip_provider: "projects/1060696143196/locations/global/workloadIdentityPools/pairdemopool/providers/oidcpairprovider"
}
advertiser_bucket_attestation_info {
  project_id: "atlantean-stone-429613-b1"
  wip_provider: "projects/1060696143196/locations/global/workloadIdentityPools/pairdemopool/providers/oidcpairprovider"
}

cc/worker_runner/worker_runner.cc:main:450|32: Processing match job.
cc/worker_runner/worker_runner.cc:main:463|32: Successfully exported matches to matchList859fa370-a8d5-4fbe-a19a-4880ee130361.csv
```

Once this job is complete, we can view the output matched list to see the hashed/encrypted IDs that match between the 2 lists. `cc/experimental/print_matched_ids.sh` can help print out 1) the generated publisher mapping 2) the input advertiser list and 3) the generated mapping.

```
PubMapping:
aieBxLwHHE71zCB@tdSfoGNyV2aCDI.com,17E301D2-E52E-E616-6A5B-59CB46835EF7
YnJgnIbrdL69w14a5@Y6F1RjpobV6KeYQD7.com,17E301D2-E52E-E617-06FC-C9300363DC68
kemPTmk3QGA6bUGapGhppROUuIOuprf5Y8ZV7l2x@K4d4QNIS7F3syWtBF.6fKm2NNcH6ds7Uy33gjKB.com,17E301D2-E52E-E618-A6F6-CE2857F8167D
8wCy.YIPZ289C9h@9TNJsrodaWIE8K.com,17E301D2-E52E-E619-74BC-F0C3E1CF53CC
xR8vFSqnGlPr5itlEfAWrtSoNYB6P8g6Frx8bv@vzhXMbiMiHsGLuzAhkOAZy5M4xpVWa3A4lIuy.com,17E301D2-E52E-E61A-F052-57626384D720
mjvQiILd.4KdaYuSDOgaJ@3cXwmYB4ZvjoFF75qtOm.com,17E301D2-E52E-E61B-C682-89EDD669EE7A
fJFRpIrPwCYiKHwotPVkHpkHWj4hT0@NjdNGTZuaSzo.yBm0JZpQoXJi2sj.com,17E301D2-E52E-E61C-390E-25F76A2EA3DE
8n52DJymgziO8B5R6jwjqLk@xkQRrISnQykT2D87pSWJqgl.com,17E301D2-E52E-E61D-F58A-ED3FF9D2D507
g0PhlpnPmVCiCTdZikr7bCCAvqVzWc@m.WghcIY0l0TSZ3EEhy9YFPnSbbE.com,17E301D2-E52E-E61E-FA19-C970A73BE1B7
sl36QLahWQ0SlBrkbkdE84ChLI5grN0U2WbD8bsIC8Z@.BIYuGQhCI7YaRY85GEiuG.17EB47izKQkhq8MeaHVl.com,17E301D2-E52E-E61F-EE2A-30F4AEF5174B
zAiSv40NG1UFTPKQCXJDkZEozKvjsBV5rMNhBgANf08A@U7whcvsx86NdKtJt4Jxr3WWVJfxy8nN3lkDFVH2AlFq.com,17E301D2-E52E-E620-6F31-1A3A8B4B1382
d8PzWYnjzkVxaSv3ljSY4MaDX3cqSavrf36A0h3@9MnVjqGX.YBaP.XXTGxnf0OMc.NgNv85CP1Rpp.com,17E301D2-E52E-E621-E695-222CE5F0B9C7
AdvList:
LJkhoTFOtAuZFtukGKJjX4cUm5PgYZAJ@u5IY.TPp2HBtYRGqwUX3LZ.Qo33HhdKE.com
mjvQiILd.4KdaYuSDOgaJ@3cXwmYB4ZvjoFF75qtOm.com
kemPTmk3QGA6bUGapGhppROUuIOuprf5Y8ZV7l2x@K4d4QNIS7F3syWtBF.6fKm2NNcH6ds7Uy33gjKB.com
8n52DJymgziO8B5R6jwjqLk@xkQRrISnQykT2D87pSWJqgl.com
8wCy.YIPZ289C9h@9TNJsrodaWIE8K.com
Ck3DbWwOEQsNzoy5lZpj4mSZeIrhRevgi8@MtNKnpvTtUdYyhqDKMgDg6ZVKXSzhXij8b8.com
d8PzWYnjzkVxaSv3ljSY4MaDX3cqSavrf36A0h3@9MnVjqGX.YBaP.XXTGxnf0OMc.NgNv85CP1Rpp.com
xR8vFSqnGlPr5itlEfAWrtSoNYB6P8g6Frx8bv@vzhXMbiMiHsGLuzAhkOAZy5M4xpVWa3A4lIuy.com
zAiSv40NG1UFTPKQCXJDkZEozKvjsBV5rMNhBgANf08A@U7whcvsx86NdKtJt4Jxr3WWVJfxy8nN3lkDFVH2AlFq.com
M8CpT7LB1NE8ltuIb8mdWoYp4LPruj@gS.x3OGtNa3sSyAq8ka79doo99teeaQ.com
fJFRpIrPwCYiKHwotPVkHpkHWj4hT0@NjdNGTZuaSzo.yBm0JZpQoXJi2sj.com
sl36QLahWQ0SlBrkbkdE84ChLI5grN0U2WbD8bsIC8Z@.BIYuGQhCI7YaRY85GEiuG.17EB47izKQkhq8MeaHVl.com
Matched Encrypted IDs
17E301D2-E52E-E61B-C682-89EDD669EE7A
17E301D2-E52E-E618-A6F6-CE2857F8167D
17E301D2-E52E-E61D-F58A-ED3FF9D2D507
17E301D2-E52E-E619-74BC-F0C3E1CF53CC
17E301D2-E52E-E621-E695-222CE5F0B9C7
17E301D2-E52E-E61A-F052-57626384D720
17E301D2-E52E-E620-6F31-1A3A8B4B1382
17E301D2-E52E-E61C-390E-25F76A2EA3DE
17E301D2-E52E-E61F-EE2A-30F4AEF5174B
```

## Teardown

Once testing is complete, we should teardown all the resources using Terraform. We can leverage the `build_and_deploy.sh` script to do this for us:
`terraform/gcp/deploy/build_and_deploy.sh --environment=<environment_name> --action=destroy`

NOTE: You'll need to set `spanner_database_deletion_protection = false` in the tfvars file and rerun `build_and_deploy.sh` with `apply` and then run `destroy` 
