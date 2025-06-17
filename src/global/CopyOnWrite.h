#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include <memory>
#include <variant>
#include <utility> // For std::move and std::visit

namespace mm {

template <typename T>
class CopyOnWrite {
public:
  // Constructor for read-only data
  explicit CopyOnWrite(std::shared_ptr<const T> data)
      : m_data_variant(std::move(data)) {}

  // Constructor for writable data
  explicit CopyOnWrite(std::shared_ptr<T> data)
      : m_data_variant(std::move(data)) {}

  // Default constructor: initializes with a default-constructed T
  CopyOnWrite() : m_data_variant(std::make_shared<T>()) {}

  // Returns a const shared_ptr to the data.
  std::shared_ptr<const T> get() const {
    return std::visit(
        [](const auto& ptr) -> std::shared_ptr<const T> { return ptr; },
        m_data_variant);
  }

  // Returns a mutable shared_ptr to the data.
  // Performs a copy if the data is not already writable, or if it is writable
  // but shared (use_count > 1).
  std::shared_ptr<T> getMutable() {
    if (std::holds_alternative<std::shared_ptr<const T>>(m_data_variant)) {
      auto const_ptr = std::get<std::shared_ptr<const T>>(m_data_variant);
      // Perform a deep copy from const source
      auto writable_ptr = std::make_shared<T>(*const_ptr);
      m_data_variant = writable_ptr;
      return writable_ptr;
    } else { // Already holds std::shared_ptr<T>
      std::shared_ptr<T>& current_writable_ptr = std::get<std::shared_ptr<T>>(m_data_variant);
      if (current_writable_ptr && current_writable_ptr.use_count() > 1) { // Added null check for current_writable_ptr
        // If shared, perform a copy to ensure unique ownership for mutation
        auto new_writable_ptr = std::make_shared<T>(*current_writable_ptr);
        m_data_variant = new_writable_ptr;
        return new_writable_ptr;
      }
      // If not shared (use_count == 1 or ptr is null), return the existing writable pointer
      return current_writable_ptr;
    }
  }

  // Converts a writable instance to read-only.
  // If the instance is already read-only, this is a no-op.
  void finalize() {
    if (std::holds_alternative<std::shared_ptr<T>>(m_data_variant)) {
      auto writable_ptr = std::get<std::shared_ptr<T>>(m_data_variant);
      // Cast to const T and store back.
      m_data_variant = std::static_pointer_cast<const T>(writable_ptr);
    }
  }

  // Checks if the current instance is writable.
  bool isWritable() const {
    return std::holds_alternative<std::shared_ptr<T>>(m_data_variant);
  }

  bool operator==(const CopyOnWrite<T>& other) const {
    std::shared_ptr<const T> p1 = get();
    std::shared_ptr<const T> p2 = other.get();
    if (p1 == p2) { // Handles both being nullptr or pointing to the exact same instance
      return true;
    }
    if (p1 && p2) { // Both are non-nullptr, compare the pointed-to objects
      return *p1 == *p2; // Requires T to have operator==
    }
    return false; // One is nullptr and the other is not
  }

  bool operator!=(const CopyOnWrite<T>& other) const {
    return !(*this == other);
  }

private:
  std::variant<std::shared_ptr<const T>, std::shared_ptr<T>> m_data_variant;
};

} // namespace mm
