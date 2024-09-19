TEE Based SCP Job Processor Repo

# Projects

Currently, the only project in this repo is the PAIR project which is a worker that matches a publisher's PII which has a corresponding SID (Surrogate ID) mapped to it with an advertiser's PII. Only the PIIs that match across both sets have their SIDs exported.

# Running

The worker binary can be found in cc/worker_runner/worker_runner.cc. This binary can be built using the build_worker.sh found in pairworker.
Once the worker binary is built, a TEE processor can be brought up with the terraform config found in terragorm/gcp/environments/dev/. A convenience script can be found in terraform/gcp/deploy/build_and_deploy.sh which performs additional steps to get the configurations setup correctly.

# Contribution

Please see CONTRIBUTING.md for details.