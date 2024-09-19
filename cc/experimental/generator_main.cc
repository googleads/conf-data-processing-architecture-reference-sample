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

#include "cc/core/async_executor/src/async_executor.h"
#include "cc/public/cpio/adapters/blob_storage_client/src/blob_storage_client.h"
#include "cc/public/cpio/interface/blob_storage_client/blob_storage_client_interface.h"
#include "cc/public/cpio/interface/cpio.h"
#include "cc/publisher_list_generator/generator/src/generator.h"
#include "cc/publisher_list_generator/id_encryptor/src/random_id_encryptor.h"
#include "cc/publisher_list_generator/publisher_list_fetcher/src/gcs_publisher_list_fetcher.h"
#include "cc/publisher_list_generator/publisher_list_fetcher/src/local_publisher_list_fetcher.h"
#include "cc/publisher_list_generator/publisher_mapping_uploader/src/gcs_publisher_mapping_uploader.h"

using google::scp::core::AsyncExecutor;
using google::scp::core::common::Uuid;
using google::scp::cpio::BlobStorageClientFactory;
using google::scp::cpio::BlobStorageClientInterface;
using google::scp::cpio::Cpio;
using google::scp::cpio::CpioOptions;
using google::scp::cpio::LogOption;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::string;

#define CALL_OR_RETURN_FAILURE(ptr, method)          \
  if (auto res = ptr->method(); !res.Successful()) { \
    return EXIT_FAILURE;                             \
  }

const uint32_t kNumThreads = std::thread::hardware_concurrency() * 2;
constexpr size_t kQueueSize = 10000000;

namespace google::pair::publisher_list_generator {

int Generate(int argc, char** argv) {
  auto cpu_async_executor = make_shared<AsyncExecutor>(kNumThreads, kQueueSize);
  CALL_OR_RETURN_FAILURE(cpu_async_executor, Init);
  CALL_OR_RETURN_FAILURE(cpu_async_executor, Run);
  auto io_async_executor = make_shared<AsyncExecutor>(kNumThreads, kQueueSize);
  CALL_OR_RETURN_FAILURE(io_async_executor, Init);
  CALL_OR_RETURN_FAILURE(io_async_executor, Run);

  CpioOptions cpio_options;
  cpio_options.log_option = LogOption::kConsoleLog;
  cpio_options.cpu_async_executor = cpu_async_executor;
  cpio_options.io_async_executor = io_async_executor;
  if (auto init_result = Cpio::InitCpio(cpio_options);
      !init_result.Successful()) {
    return EXIT_FAILURE;
  }

  shared_ptr<BlobStorageClientInterface> blob_storage_client(
      BlobStorageClientFactory::Create().release());

  CALL_OR_RETURN_FAILURE(blob_storage_client, Init);
  CALL_OR_RETURN_FAILURE(blob_storage_client, Run);
  Generator<string, Uuid> generator(
      make_unique<GcsPublisherListFetcher>(blob_storage_client),
      make_unique<RandomIdEncryptor>(cpu_async_executor),
      make_unique<GcsPublisherMappingUploader>(blob_storage_client),
      blob_storage_client);
  auto* input_bucket = argv[0];
  auto* list_name = argv[1];
  auto* metadata_name = argv[2];
  auto* mapping_name = argv[3];
  auto generation_result = generator.GeneratePublisherList(
      {input_bucket, list_name, metadata_name, mapping_name});

  CALL_OR_RETURN_FAILURE(blob_storage_client, Stop);
  Cpio::ShutdownCpio(CpioOptions());
  CALL_OR_RETURN_FAILURE(cpu_async_executor, Stop);
  CALL_OR_RETURN_FAILURE(io_async_executor, Stop);

  if (!generation_result.Successful()) {
    std::cout << "Failed generation "
              << google::scp::core::errors::GetErrorMessage(
                     generation_result.status_code)
              << std::endl;
    return EXIT_FAILURE;
  }
  std::cout << "Succeeded!" << std::endl;
  return EXIT_SUCCESS;
}

}  // namespace google::pair::publisher_list_generator

// input_bucket_name
// input_list_name
// input_metadata_name
// generated_list_name
int main(int argc, char** argv) {
  if (argc != 5) {
    std::cerr << "Expected 4 args but got " << argc << std::endl;
    for (int i = 0; i < argc; i++) {
      std::cerr << argv[i] << std::endl;
    }
    return EXIT_FAILURE;
  }
  return google::pair::publisher_list_generator::Generate(argc, argv + 1);
}
