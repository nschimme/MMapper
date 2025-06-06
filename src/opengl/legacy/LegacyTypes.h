#pragma once
#include <memory> // For std::shared_ptr, std::weak_ptr

namespace Legacy {
    class Functions; // Forward declaration
    using SharedFunctions = std::shared_ptr<Functions>;
    using WeakFunctions = std::weak_ptr<Functions>;
} // namespace Legacy
