#pragma once

#include <span>
#include <vector>

namespace motis::gbfs {

template <typename T>
struct partition {
  explicit partition(T const n) : n_{n} {
    partition_.resize(n);
    for (auto i = T{0}; i < n; ++i) {
      partition_[i] = i;
    }
    if (n != 0) {
      // initially there's only one set ending at n-1
      set_ends_.push_back(n - 1);
    }
  }

  void refine(std::span<T const> const s) {
    if (s.empty()) {
      return;
    }

    // mark elements in s
    auto in_s = std::vector<bool>(n_, false);
    for (auto const elem : s) {
      in_s[elem] = true;
    }

    // process each existing set
    auto current_start = T{0};
    auto new_set_ends = std::vector<T>{};
    new_set_ends.reserve(2 * set_ends_.size());

    for (auto const set_end : set_ends_) {
      // count elements in current set that are in s
      auto count = T{0};
      for (auto i = current_start; i <= set_end; ++i) {
        if (in_s[partition_[i]]) {
          ++count;
        }
      }

      auto const set_size = set_end - current_start + 1;
      // if split is needed (some but not all elements are in s)
      if (count != 0 && count != set_size) {
        // partition the set into two parts
        auto split_pos = current_start;
        for (auto i = current_start; i <= set_end; ++i) {
          if (in_s[partition_[i]]) {
            // move element to front of split
            if (i != split_pos) {
              std::swap(partition_[i], partition_[split_pos]);
            }
            ++split_pos;
          }
        }

        // add end positions for both new sets
        new_set_ends.push_back(split_pos - 1);
        new_set_ends.push_back(set_end);
      } else {
        // no split needed, keep original set
        new_set_ends.push_back(set_end);
      }

      current_start = set_end + 1;
    }

    set_ends_ = std::move(new_set_ends);
  }

  std::vector<std::vector<T>> get_sets() const {
    auto result = std::vector<std::vector<T>>{};
    result.reserve(set_ends_.size());

    auto current_start = T{0};
    for (auto const set_end : set_ends_) {
      auto set = std::vector<T>{};
      set.reserve(set_end - current_start + 1);
      for (auto i = current_start; i <= set_end; ++i) {
        set.push_back(partition_[i]);
      }
      result.push_back(std::move(set));
      current_start = set_end + 1;
    }

    return result;
  }

  // the number of elements in the partition - the original set
  // contains the elements 0, 1, ..., n - 1
  T const n_;

  // stores the elements grouped by sets - the elements of each set are
  // stored contiguously, e.g. "0345789" could be {{0, 3, 4}, {5}, {7, 8, 9}}
  // or {{0}, {3, 4}, {5}, {7, 8, 9}}, depending on set_ends_.
  std::vector<T> partition_;

  // stores the end index of each set in partition_
  std::vector<T> set_ends_;
};

}  // namespace motis::gbfs
