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

#include <fstream>
#include <mutex>
#include <thread>

#include "absl/container/flat_hash_set.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "absl/strings/str_format.h"
#include "cc/core/common/uuid/src/uuid.h"
#include "cc/publisher_list_generator/proto/publisher_pair_list.pb.h"

using std::string;

namespace google::pair::matcher {

namespace {
string GetRandomEmail(absl::BitGen& bitgen) {
  constexpr char kAllowedChars[] =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.";
  int length = absl::Uniform(bitgen, 30, 100);
  string s;
  s.reserve(length);
  for (int i = 0; i < length; i++) {
    if (i == length / 2) {
      s += '@';
      continue;
    }
    auto index =
        absl::Uniform(bitgen, static_cast<size_t>(0), sizeof(kAllowedChars));
    s += kAllowedChars[index];
  }
  s += ".com";
  return s;
}
}  // namespace

int MakeEmailFile(int argc, char** argv) {
  std::ofstream file(argv[0]);
  std::mutex file_mutex;
  long num_emails = std::strtol(argv[1], nullptr, 10);
  std::vector<std::thread> threads;
  int num_threads = num_emails < 30 ? 1 : 30;
  for (auto i = 0; i < num_threads; i++) {
    threads.emplace_back(
        [i = i, num_emails, num_threads, &file_mutex, &file]() {
          string emails_string;
          absl::BitGen bitgen;
          int num_emails_for_thread = num_emails / num_threads;
          if (i == num_threads - 1) {
            num_emails_for_thread += num_emails % num_threads;
          }
          for (auto i = 0; i < num_emails_for_thread; i++) {
            emails_string += GetRandomEmail(bitgen);
            emails_string += '\n';
          }
          std::scoped_lock lock(file_mutex);
          file << emails_string;
        });
  }
  for (auto& t : threads) t.join();
  std::cout << "Succeeded" << std::endl;
  return EXIT_SUCCESS;
}

int UploadPubMapping(int argc, char** argv) {
  std::ifstream file(argv[0]);
  unsigned long num_emails = std::strtoul(argv[1], nullptr, 10);
  absl::BitGen bitgen;
  std::string mapping;
  std::vector<std::string> lines;
  for (string email; std::getline(file, email);) {
    lines.emplace_back(std::move(email));
  }
  absl::flat_hash_set<size_t> seen_indices;
  for (unsigned long i = 0; i < num_emails; i++) {
    size_t index;
    do {
      index = absl::Uniform(bitgen, static_cast<size_t>(0), lines.size());
    } while (seen_indices.contains(index));
    absl::StrAppend(
        &mapping, lines[index], ",",
        scp::core::common::ToString(scp::core::common::Uuid::GenerateUuid()),
        "\n");
    seen_indices.insert(index);
    if ((i % 1000000) == 0) std::cout << "1,000,000 done" << std::endl;
  }
  string command =
      absl::StrFormat("gcloud storage cp - gs://%s/%s", argv[2], argv[3]);
  auto* ofile = popen(command.c_str(), "w");
  if (ofile == nullptr) {
    std::cerr << "Failed opening" << std::endl;
    return EXIT_FAILURE;
  }
  if (fwrite(mapping.c_str(), sizeof(char), mapping.length(), ofile) < 0) {
    std::cerr << "Failed piping" << std::endl;
    return EXIT_FAILURE;
  }
  if (pclose(ofile) < 0) {
    std::cerr << "Failed closing" << std::endl;
    return EXIT_FAILURE;
  }
  std::cout << "Succeeded" << std::endl;
  return EXIT_SUCCESS;
}

int UploadPubOrAdvList(int argc, char** argv) {
  std::ifstream file(argv[0]);
  unsigned long num_emails = std::strtoul(argv[1], nullptr, 10);
  absl::BitGen bitgen;
  std::vector<std::string> lines;
  for (string email; std::getline(file, email);) {
    lines.emplace_back(std::move(email));
  }
  string email_csv;
  absl::flat_hash_set<size_t> seen_indices;
  for (unsigned long i = 0; i < num_emails; i++) {
    size_t index;
    do {
      index = absl::Uniform(bitgen, static_cast<size_t>(0), lines.size());
    } while (seen_indices.contains(index));
    email_csv += lines[index];
    email_csv += "\n";
    seen_indices.insert(index);
    if ((seen_indices.size() % 1000000) == 0)
      std::cout << "1,000,000 done" << std::endl;
  }
  string command =
      absl::StrFormat("gcloud storage cp - gs://%s/%s", argv[2], argv[3]);
  auto* ofile = popen(command.c_str(), "w");
  if (ofile == nullptr) {
    std::cerr << "Failed opening" << std::endl;
    return EXIT_FAILURE;
  }
  if (fwrite(email_csv.c_str(), sizeof(char), email_csv.length(), ofile) < 0) {
    std::cerr << "Failed piping" << std::endl;
    return EXIT_FAILURE;
  }
  if (pclose(ofile) < 0) {
    std::cerr << "Failed closing" << std::endl;
    return EXIT_FAILURE;
  }
  std::cout << "Succeeded" << std::endl;
  return EXIT_SUCCESS;
}

}  // namespace google::pair::matcher

// Mode 1 - email gen, 2 - pub/adv input list, 3 - pub mapping (for skipping
// generator step)
// file name
// number of emails
// Modes 2 & 3:
//   bucket name
//   object name
int main(int argc, char** argv) {
  long mode = std::strtol(argv[1], nullptr, 10);
  switch (mode) {
    case 1:
    case 2:
    case 3:
      break;
    default:
      std::cerr << "Bad mode: " << mode << std::endl;
      return EXIT_FAILURE;
  }
  if (mode == 1) {
    if (argc < 3) {
      std::cerr << "Expected 2 args but got " << argc - 1 << std::endl;
      for (int i = 0; i < argc; i++) {
        std::cerr << argv[i] << std::endl;
      }
      return EXIT_FAILURE;
    }
    return google::pair::matcher::MakeEmailFile(argc, argv + 2);
  } else if (mode == 2) {
    if (argc != 6) {
      std::cerr << "Expected 5 args but got " << argc - 1 << std::endl;
      return EXIT_FAILURE;
    }
    return google::pair::matcher::UploadPubOrAdvList(argc, argv + 2);
  } else if (mode == 3) {
    if (argc != 6) {
      std::cerr << "Expected 5 args but got " << argc - 1 << std::endl;
      return EXIT_FAILURE;
    }
    return google::pair::matcher::UploadPubMapping(argc, argv + 2);
  }
  return EXIT_FAILURE;
}
