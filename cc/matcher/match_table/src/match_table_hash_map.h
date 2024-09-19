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

#pragma once

#include <memory>
#include <mutex>

#include "absl/container/flat_hash_map.h"

#include "error_codes.h"
#include "match_table.h"

namespace google::pair::matcher {

template <typename K, typename V>
class MatchTableHashMap : public MatchTable<K, V> {
 public:
  scp::core::ExecutionResult AddElement(const K& key, const V& value) override {
    std::lock_guard lock(data_mutex_);

    if (data_.contains(key)) {
      return scp::core::FailureExecutionResult(
          errors::MATCH_TABLE_ELEMENT_ALREADY_EXISTS);
    }

    data_[key] = std::make_unique<ValueInfo>(value);
    return scp::core::SuccessExecutionResult();
  }

  scp::core::ExecutionResultOr<V> MarkMatched(const K& key) override {
    std::lock_guard lock(data_mutex_);

    if (data_.contains(key)) {
      data_[key]->MarkMatched();
      return data_[key]->GetValue();
    }

    return scp::core::FailureExecutionResult(
        errors::MATCH_TABLE_ELEMENT_DOES_NOT_EXIST);
  }

  void VisitMatched(
      typename MatchTable<K, V>::VisitorCallback visitor) override {
    std::lock_guard lock(data_mutex_);

    for (const auto& [key, val] : data_) {
      if (val->IsMatched()) {
        visitor(key, val->GetValue());
      }
    }
  }

 private:
  /**
   * @brief Struct to hold the value information.
   * It contains the actual value and whether this value has been matched.
   *
   */
  struct ValueInfo {
    V value;
    bool is_matched;

    ValueInfo(const V& value, bool is_matched = false) {
      this->value = value;
      this->is_matched = is_matched;
    }

    void MarkMatched() { this->is_matched = true; }

    V& GetValue() { return value; }

    bool IsMatched() const { return is_matched; }
  };

  /**
   * @brief Map containing the key value pairs and match info.
   *
   */
  absl::flat_hash_map<K, std::unique_ptr<ValueInfo>> data_;
  mutable std::mutex data_mutex_;
};
}  // namespace google::pair::matcher
