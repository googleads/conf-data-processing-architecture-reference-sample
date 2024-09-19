// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <iostream>
#include <optional>

#include <google/protobuf/util/time_util.h>

#include "absl/debugging/failure_signal_handler.h"
#include "cc/common/attestation/src/attestation_info.h"
#include "cc/core/common/global_logger/src/global_logger.h"
#include "cc/matcher/match_worker/src/match_worker.h"
#include "cc/publisher_list_generator/generator/src/generator.h"
#include "cc/publisher_list_generator/id_encryptor/src/random_id_encryptor.h"
#include "cc/publisher_list_generator/publisher_list_fetcher/src/gcs_publisher_list_fetcher.h"
#include "cc/publisher_list_generator/publisher_mapping_uploader/src/gcs_publisher_mapping_uploader.h"
#include "cc/worker_runner/pair_job_data.pb.h"
#include "core/async_executor/src/async_executor.h"
#include "google/protobuf/util/json_util.h"
#include "public/core/interface/errors.h"
#include "public/cpio/interface/blob_storage_client/blob_storage_client_interface.h"
#include "public/cpio/interface/cpio.h"
#include "public/cpio/interface/job_client/job_client_interface.h"
#include "public/cpio/interface/job_client/type_def.h"
#include "public/cpio/utils/configuration_fetcher/interface/configuration_fetcher_interface.h"
#include "public/cpio/utils/configuration_fetcher/src/configuration_fetcher.h"
#include "public/cpio/utils/job_lifecycle_helper/src/job_lifecycle_helper.h"
#include "public/cpio/utils/metric_instance/src/metric_instance_factory.h"

using google::cmrt::sdk::common::v1::CloudIdentityInfo;
using google::cmrt::sdk::job_lifecycle_helper::v1::JobLifecycleHelperOptions;
using google::cmrt::sdk::job_lifecycle_helper::v1::MarkJobCompletedRequest;
using google::cmrt::sdk::job_lifecycle_helper::v1::PrepareNextJobResponse;
using google::cmrt::sdk::job_service::v1::JobStatus;
using google::pair::common::BlobStreamer;
using google::pair::common::BuildGcpCloudIdentityInfo;
using google::pair::job::JobType;
using google::pair::job::PairJobData;
using google::pair::matcher::MatchWorker;
using google::pair::publisher_list_generator::GcsPublisherListFetcher;
using google::pair::publisher_list_generator::GcsPublisherMappingUploader;
using google::pair::publisher_list_generator::GeneratePublisherListRequest;
using google::pair::publisher_list_generator::Generator;
using google::pair::publisher_list_generator::RandomIdEncryptor;
using google::protobuf::util::JsonStringToMessage;
using google::protobuf::util::TimeUtil;
using google::scp::core::AsyncExecutor;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::GetErrorMessage;
using google::scp::core::LogLevel;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::common::Uuid;
using google::scp::cpio::AutoScalingClientFactory;
using google::scp::cpio::AutoScalingClientInterface;
using google::scp::cpio::AutoScalingClientOptions;
using google::scp::cpio::BlobStorageClientFactory;
using google::scp::cpio::BlobStorageClientInterface;
using google::scp::cpio::ConfigurationFetcher;
using google::scp::cpio::ConfigurationFetcherInterface;
using google::scp::cpio::Cpio;
using google::scp::cpio::CpioOptions;
using google::scp::cpio::GetConfigurationRequest;
using google::scp::cpio::InstanceClientFactory;
using google::scp::cpio::InstanceClientInterface;
using google::scp::cpio::InstanceClientOptions;
using google::scp::cpio::JobClientFactory;
using google::scp::cpio::JobClientInterface;
using google::scp::cpio::JobClientOptions;
using google::scp::cpio::JobLifecycleHelper;
using google::scp::cpio::JobLifecycleHelperInterface;
using google::scp::cpio::LogOption;
using google::scp::cpio::MetricClientFactory;
using google::scp::cpio::MetricClientInterface;
using google::scp::cpio::MetricClientOptions;
using google::scp::cpio::MetricInstanceFactory;
using google::scp::cpio::MetricInstanceFactoryInterface;
using google::scp::cpio::ParameterClientFactory;
using google::scp::cpio::ParameterClientInterface;
using google::scp::cpio::ParameterClientOptions;
using std::make_shared;
using std::make_unique;
using std::move;
using std::nullopt;
using std::optional;
using std::pair;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::steady_clock;
using std::this_thread::sleep_for;

constexpr char kWorkerRunnerMain[] = "WorkerRunnerMain";
constexpr milliseconds kLogPeriod = milliseconds(5000);

shared_ptr<AsyncExecutor> cpu_async_executor;
shared_ptr<AsyncExecutor> io_async_executor;
shared_ptr<ConfigurationFetcherInterface> configuration_fetcher;
shared_ptr<AutoScalingClientInterface> auto_scaling_client;
shared_ptr<MetricClientInterface> metric_client;
shared_ptr<MetricInstanceFactoryInterface> metric_instance_factory;
shared_ptr<JobClientInterface> job_client;
unique_ptr<JobLifecycleHelperInterface> job_lifecycle_helper;
shared_ptr<BlobStorageClientInterface> blob_storage_client;

void StopAllClients() {
  if (blob_storage_client) {
    blob_storage_client->Stop();
  }
  if (job_lifecycle_helper) {
    job_lifecycle_helper->Stop();
  }
  if (job_client) {
    job_client->Stop();
  }
  if (metric_client) {
    metric_client->Stop();
  }
  if (auto_scaling_client) {
    auto_scaling_client->Stop();
  }
  if (configuration_fetcher) {
    configuration_fetcher->Stop();
  }
  if (io_async_executor) {
    io_async_executor->Stop();
  }
  if (cpu_async_executor) {
    cpu_async_executor->Stop();
  }
  Cpio::ShutdownCpio(CpioOptions());
}

ExecutionResult CreateInitAndRunAutoScalingAndMetricInstanceFactory() {
  AutoScalingClientOptions auto_scaling_client_options;
  ASSIGN_OR_RETURN(
      auto_scaling_client_options.instance_table_name,
      configuration_fetcher->GetAutoScalingClientInstanceTableNameSync({}));

  ASSIGN_OR_RETURN(
      auto_scaling_client_options.gcp_spanner_instance_name,
      configuration_fetcher->GetAutoScalingClientSpannerInstanceNameSync({}));
  ASSIGN_OR_RETURN(
      auto_scaling_client_options.gcp_spanner_database_name,
      configuration_fetcher->GetAutoScalingClientSpannerDatabaseNameSync({}));
  auto_scaling_client =
      AutoScalingClientFactory::Create(auto_scaling_client_options);
  RETURN_IF_FAILURE(auto_scaling_client->Init());
  RETURN_IF_FAILURE(auto_scaling_client->Run());

  metric_client = MetricClientFactory::Create(MetricClientOptions());
  RETURN_IF_FAILURE(metric_client->Init());
  RETURN_IF_FAILURE(metric_client->Run());
  metric_instance_factory = make_unique<MetricInstanceFactory>(
      cpu_async_executor.get(), metric_client.get());
  return SuccessExecutionResult();
}

ExecutionResult CreateJobLifecycleHelper() {
  // Setup Job Client.
  JobClientOptions client_options;
  ASSIGN_OR_RETURN(client_options.job_queue_name,
                   configuration_fetcher->GetJobClientJobQueueNameSync({}));
  ASSIGN_OR_RETURN(client_options.job_table_name,
                   configuration_fetcher->GetJobClientJobTableNameSync({}));
  ASSIGN_OR_RETURN(
      client_options.gcp_spanner_instance_name,
      configuration_fetcher->GetGcpJobClientSpannerInstanceNameSync({}));
  ASSIGN_OR_RETURN(
      client_options.gcp_spanner_database_name,
      configuration_fetcher->GetGcpJobClientSpannerDatabaseNameSync({}));
  SCP_INFO(kWorkerRunnerMain, kZeroUuid,
           "Starting job client with job_queue_name=%s, job_table_name=%s, "
           "gcp_spanner_instance_name=%s, gcp_spanner_database_name=%s",
           client_options.job_queue_name.c_str(),
           client_options.job_table_name.c_str(),
           client_options.gcp_spanner_instance_name.c_str(),
           client_options.gcp_spanner_database_name.c_str());

  job_client = JobClientFactory::Create(move(client_options));
  RETURN_IF_FAILURE(job_client->Init());
  RETURN_IF_FAILURE(job_client->Run());
  JobLifecycleHelperOptions job_lifecycle_helper_options;
  size_t retry_limit =
      configuration_fetcher->GetJobLifecycleHelperRetryLimitSync({}).value_or(
          3);
  job_lifecycle_helper_options.set_retry_limit(retry_limit);
  size_t visibility_timeout =
      configuration_fetcher
          ->GetJobLifecycleHelperVisibilityTimeoutExtendTimeSync({})
          .value_or(5 * 60);
  *job_lifecycle_helper_options
       .mutable_visibility_timeout_extend_time_seconds() =
      TimeUtil::SecondsToDuration(visibility_timeout);

  size_t processing_timeout =
      configuration_fetcher->GetJobLifecycleHelperJobProcessingTimeoutSync({})
          .value_or(5 * 60);
  *job_lifecycle_helper_options.mutable_job_processing_timeout_seconds() =
      TimeUtil::SecondsToDuration(processing_timeout);

  size_t worker_sleep_time =
      configuration_fetcher
          ->GetJobLifecycleHelperJobExtendingWorkerSleepTimeSync({})
          .value_or(30);
  *job_lifecycle_helper_options
       .mutable_job_extending_worker_sleep_time_seconds() =
      TimeUtil::SecondsToDuration(worker_sleep_time);
  ASSIGN_OR_RETURN(
      *job_lifecycle_helper_options.mutable_current_instance_resource_name(),
      configuration_fetcher->GetCurrentInstanceResourceNameSync({}));
  ASSIGN_OR_RETURN(
      *job_lifecycle_helper_options.mutable_scale_in_hook_name(),
      configuration_fetcher->GetAutoScalingClientScaleInHookNameSync({}));

  job_lifecycle_helper = make_unique<JobLifecycleHelper>(
      job_client.get(), auto_scaling_client.get(),
      metric_instance_factory.get(), std::move(job_lifecycle_helper_options));
  RETURN_IF_FAILURE(job_lifecycle_helper->Init());
  return job_lifecycle_helper->Run();
}

string GetListName() {
  auto ms = duration_cast<milliseconds>(steady_clock::now().time_since_epoch())
                .count();
  return string("PubXMapping") + std::to_string(ms) + string(".rawproto");
}

string GetMatchName() {
  auto ms = duration_cast<milliseconds>(steady_clock::now().time_since_epoch())
                .count();
  // TODO change to .textproto once the output format is changed.
  return string("PubXAdvYMatch") + std::to_string(ms) + string(".csv");
}

optional<CloudIdentityInfo> GetPublisherProjectIdAndWipProvider(
    const PairJobData& pair_job_data) {
  if (pair_job_data.has_publisher_bucket_attestation_info()) {
    return BuildGcpCloudIdentityInfo(
        pair_job_data.publisher_bucket_attestation_info().project_id(),
        pair_job_data.publisher_bucket_attestation_info().wip_provider());
  }
  return std::nullopt;
}

optional<CloudIdentityInfo> GetAdvertiserProjectIdAndWipProvider(
    const PairJobData& pair_job_data) {
  if (pair_job_data.has_advertiser_bucket_attestation_info()) {
    return BuildGcpCloudIdentityInfo(
        pair_job_data.advertiser_bucket_attestation_info().project_id(),
        pair_job_data.advertiser_bucket_attestation_info().wip_provider());
  }
  return std::nullopt;
}

int main(int argc, char* argv[]) {
  // Install signal handler for printing verbose core dumps.
  // https://github.com/abseil/abseil-cpp/blob/master/absl/debugging/failure_signal_handler.h
  absl::InstallFailureSignalHandler(absl::FailureSignalHandlerOptions());

  cpu_async_executor = make_shared<AsyncExecutor>(16, 10000000);
  auto result = cpu_async_executor->Init();
  if (!result.Successful()) {
    SCP_ERROR(kWorkerRunnerMain, kZeroUuid, result,
              "Cannot init CPU AsyncExecutor!");
    StopAllClients();
    exit(EXIT_FAILURE);
  }
  result = cpu_async_executor->Run();
  if (!result.Successful()) {
    SCP_ERROR(kWorkerRunnerMain, kZeroUuid, result,
              "Cannot run CPU AsyncExecutor!");
    StopAllClients();
    exit(EXIT_FAILURE);
  }
  io_async_executor = make_shared<AsyncExecutor>(16, 10000000);
  result = io_async_executor->Init();
  if (!result.Successful()) {
    SCP_ERROR(kWorkerRunnerMain, kZeroUuid, result,
              "Cannot init IO AsyncExecutor!");
    StopAllClients();
    exit(EXIT_FAILURE);
  }
  result = io_async_executor->Run();
  if (!result.Successful()) {
    SCP_ERROR(kWorkerRunnerMain, kZeroUuid, result,
              "Cannot run IO AsyncExecutor!");
    StopAllClients();
    exit(EXIT_FAILURE);
  }

  // Setup Configuration Fetcher to fetch Terraform populated parameters in GCP
  // Secret Store.
  configuration_fetcher =
      make_unique<ConfigurationFetcher>(std::nullopt, std::nullopt);
  result = configuration_fetcher->Init();
  if (!result.Successful()) {
    SCP_ERROR(kWorkerRunnerMain, kZeroUuid, result,
              "Failed to Init ConfigurationFetcher");
    StopAllClients();
    exit(EXIT_FAILURE);
  }
  result = configuration_fetcher->Run();
  if (!result.Successful()) {
    SCP_ERROR(kWorkerRunnerMain, kZeroUuid, result,
              "Failed to Init ConfigurationFetcher");
    StopAllClients();
    exit(EXIT_FAILURE);
  }

  CpioOptions cpio_options;
  cpio_options.enabled_log_levels =
      configuration_fetcher->GetCommonEnabledLogLevelsSync({}).value_or(
          std::unordered_set<LogLevel>{});
  cpio_options.log_option = LogOption::kConsoleLog;
  cpio_options.cpu_async_executor = cpu_async_executor;
  cpio_options.io_async_executor = io_async_executor;
  result = Cpio::InitCpio(cpio_options);
  if (!result.Successful()) {
    SCP_ERROR(kWorkerRunnerMain, kZeroUuid, result,
              "Failed to initialize CPIO");
    StopAllClients();
    exit(EXIT_FAILURE);
  }

  result = CreateInitAndRunAutoScalingAndMetricInstanceFactory();
  if (!result.Successful()) {
    SCP_ERROR(kWorkerRunnerMain, kZeroUuid, result,
              "Failed to Create AutoScalingAndMetricInstanceFactory");
    StopAllClients();
    exit(EXIT_FAILURE);
  }
  result = CreateJobLifecycleHelper();
  if (!result.Successful()) {
    SCP_ERROR(kWorkerRunnerMain, kZeroUuid, result,
              "Failed to Create JobLifecycleHelper");
    StopAllClients();
    exit(EXIT_FAILURE);
  }

  shared_ptr<BlobStorageClientInterface> blob_storage_client(
      BlobStorageClientFactory::Create().release());

  result = blob_storage_client->Init();
  if (!result.Successful()) {
    SCP_ERROR(kWorkerRunnerMain, kZeroUuid, result,
              "Cannot init BlobStorageClient!");
    StopAllClients();
    exit(EXIT_FAILURE);
  }
  result = blob_storage_client->Run();
  if (!result.Successful()) {
    SCP_ERROR(kWorkerRunnerMain, kZeroUuid, result,
              "Cannot run BlobStorageClient!");
    StopAllClients();
    exit(EXIT_FAILURE);
  }

  Generator<string, Uuid> generator(
      make_unique<GcsPublisherListFetcher>(blob_storage_client),
      make_unique<RandomIdEncryptor>(cpu_async_executor),
      make_unique<GcsPublisherMappingUploader>(blob_storage_client),
      blob_storage_client);

  auto blob_streamer =
      make_unique<BlobStreamer>(cpu_async_executor, blob_storage_client);
  auto& blob_streamer_ref = *blob_streamer;
  result = blob_streamer->Init();
  if (!result.Successful()) {
    SCP_ERROR(kWorkerRunnerMain, kZeroUuid, result,
              "Cannot init BlobStreamer!");
    StopAllClients();
    exit(EXIT_FAILURE);
  }
  result = blob_streamer->Run();
  if (!result.Successful()) {
    SCP_ERROR(kWorkerRunnerMain, kZeroUuid, result, "Cannot run BlobStreamer!");
    StopAllClients();
    exit(EXIT_FAILURE);
  }
  MatchWorker worker(blob_storage_client, move(blob_streamer));

  while (true) {
    SCP_INFO_EVERY_PERIOD(kLogPeriod, kWorkerRunnerMain, kZeroUuid,
                          "Polling for job.");

    JobStatus job_status = JobStatus::JOB_STATUS_SUCCESS;
    ExecutionResultOr<PrepareNextJobResponse> prepare_next_job_or =
        job_lifecycle_helper->PrepareNextJobSync({});
    if (!prepare_next_job_or.Successful()) {
      SCP_ERROR_EVERY_PERIOD(kLogPeriod, kWorkerRunnerMain, kZeroUuid,
                             prepare_next_job_or.result(),
                             "PrepareNextJob didn't succeed");
      sleep_for(milliseconds(5000));
      continue;
    }

    const auto& job_response = prepare_next_job_or.value();
    if (job_response.has_job()) {
      const auto& job = job_response.job();
      SCP_INFO(kWorkerRunnerMain, kZeroUuid, "Received a job: %s",
               job.job_id().c_str());
      PairJobData pair_job_data;
      if (auto status = JsonStringToMessage(job.job_body(), &pair_job_data);
          !status.ok()) {
        SCP_ERROR(kWorkerRunnerMain, kZeroUuid,
                  FailureExecutionResult(SC_UNKNOWN),
                  "Failed parsing job_body from JSON to PairJobData %s",
                  status.ToString().c_str());
        continue;
      }
      SCP_INFO(kWorkerRunnerMain, kZeroUuid, "Parsed body: %s",
               pair_job_data.DebugString().c_str());
      if (pair_job_data.job_type() ==
          JobType::JOB_TYPE_GENERATE_PUB_PAIR_LIST) {
        SCP_INFO(kWorkerRunnerMain, kZeroUuid,
                 "Processing publisher list generation job.");
        GeneratePublisherListRequest request{
            pair_job_data.publisher_input_bucket(),
            pair_job_data.publisher_user_list_blob_path(),
            pair_job_data.publisher_metadata_blob_path(),
            pair_job_data.publisher_mapping_blob_path(),
            GetPublisherProjectIdAndWipProvider(pair_job_data)};
        result = generator.GeneratePublisherList(std::move(request));
        if (result.Successful()) {
          SCP_INFO(kWorkerRunnerMain, kZeroUuid,
                   "Successfully generated publisher mapping to %s!",
                   pair_job_data.publisher_mapping_blob_path().c_str());
        } else {
          SCP_ERROR(kWorkerRunnerMain, kZeroUuid, result,
                    "Failed generating publisher mapping");
          job_status = JobStatus::JOB_STATUS_FAILURE;
        }
      } else if (pair_job_data.job_type() == JobType::JOB_TYPE_MATCH) {
        SCP_INFO(kWorkerRunnerMain, kZeroUuid, "Processing match job.");
        result = worker.ExportMatches(
            {pair_job_data.publisher_input_bucket(),
             pair_job_data.publisher_mapping_blob_path(),
             pair_job_data.advertiser_input_bucket(),
             pair_job_data.advertiser_user_list_blob_path(),
             pair_job_data.match_output_bucket(),
             pair_job_data.match_list_blob_path(),
             GetPublisherProjectIdAndWipProvider(pair_job_data),
             GetAdvertiserProjectIdAndWipProvider(pair_job_data)});
        if (result.Successful()) {
          SCP_INFO(kWorkerRunnerMain, kZeroUuid,
                   "Successfully exported matches to %s",
                   pair_job_data.match_list_blob_path().c_str());
        } else {
          SCP_ERROR(kWorkerRunnerMain, kZeroUuid, result,
                    "Failed exporting matches");
          job_status = JobStatus::JOB_STATUS_FAILURE;
        }
      } else {
        SCP_ERROR(kWorkerRunnerMain, kZeroUuid,
                  FailureExecutionResult(SC_UNKNOWN),
                  "This is not a valid job type: %s",
                  JobType_Name(pair_job_data.job_type()).c_str());
        job_status = JobStatus::JOB_STATUS_FAILURE;
      }

      MarkJobCompletedRequest mark_job_completed_request;
      mark_job_completed_request.set_job_id(job.job_id());
      mark_job_completed_request.set_job_status(job_status);
      auto mark_job_completed_response_or =
          job_lifecycle_helper->MarkJobCompletedSync(
              mark_job_completed_request);
      if (!mark_job_completed_response_or.Successful()) {
        SCP_ERROR(kWorkerRunnerMain, kZeroUuid,
                  mark_job_completed_response_or.result(),
                  "MarkJobCompleted failed");
      }

      SCP_INFO(kWorkerRunnerMain, kZeroUuid, "Job: %s completed with status %s",
               job.job_id().c_str(), JobStatus_Name(job_status).c_str());
    }

    SCP_INFO(kWorkerRunnerMain, kZeroUuid, "Going to sleep");
    sleep_for(milliseconds(5000));
  }

  result = blob_streamer_ref.Stop();
  if (!result.Successful()) {
    SCP_ERROR(kWorkerRunnerMain, kZeroUuid, result,
              "Cannot stop BlobStreamer!");
    exit(EXIT_FAILURE);
  }

  StopAllClients();

  SCP_INFO(kWorkerRunnerMain, kZeroUuid, "This is the end!");
}
