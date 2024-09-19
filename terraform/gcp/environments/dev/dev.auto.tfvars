# Must be updated
environment = "pair-dev"
project_id  = "<project-id>"
region      = "<region>"
# Region zone is typically the same as region with some letter suffix:
# e.g.: us-west2-a
region_zone = "<region-zone>"

# Multi region location
# https://cloud.google.com/storage/docs/locations
operator_package_bucket_location = "US"

spanner_instance_config  = "<spanner-instance-config>"
spanner_processing_units = 100 # Refer to https://cloud.google.com/spanner/docs/compute-capacity

worker_image = "<path-to-worker-image>"

allowed_operator_service_account = ""

# If want to enable attestation inside TEE, onboard the pre-created SA to grant permissions.
user_provided_worker_sa_email = "<sa-email>"

frontend_service_jar = "../../jars/FrontendServiceHttpCloudFunction_deploy.jar"
worker_scale_in_jar  = "../../jars/WorkerScaleInCloudFunction_deploy.jar"

worker_logging_enabled = true
worker_container_log_redirect = true
