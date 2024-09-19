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

#include <functional>

#include "cc/public/core/interface/execution_result.h"

namespace google::pair::matcher {
/**
 * @brief This interface is used to represent a matching set.
 * Key-value pairs can be added to the set and then these keys can be marked as
 * matched by the key.
 *
 * @tparam K the key type for the elements
 * @tparam V the value type for the elements
 */
template <typename K, typename V>
class MatchTable {
 public:
  /**
   * @brief Callback provided by the caller, which gets invoked with each item
   * that was marked as matched.
   *
   */
  using VisitorCallback = std::function<void(const K& key, const V& value)>;

  /**
   * @brief Add an element (KV pair) to the table for later matching and
   * retrieval. This function should return a failure if the element
   * already exists.
   *
   * @param key the key of the element
   * @param value the value of the element
   * @return success or failure
   */
  virtual scp::core::ExecutionResult AddElement(const K& key,
                                                const V& value) = 0;

  /**
   * @brief Mark an element as matched.
   *
   * @param key the key of the element
   * @return the value that was stored for the given element by key if it exists
   * or a FailureExecutionResult
   */
  virtual scp::core::ExecutionResultOr<V> MarkMatched(const K& key) = 0;

  /**
   * @brief This method iterates over the matched elements and calls the
   * provided callback once per element with the matched element.
   *
   * @param visitor the visitor callback to invoke with each element
   */
  virtual void VisitMatched(VisitorCallback visitor) = 0;

  virtual ~MatchTable() = default;
};

}  // namespace google::pair::matcher
