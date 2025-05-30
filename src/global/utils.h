#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "NullPointerException.h"
#include "macros.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>

#include <QPointer>
#include <queue>

namespace utils {

// This mainly exists to avoid float-equal warnings,
// but it also checks that the floating point types are the same.
template<typename A, typename B>
NODISCARD constexpr bool isSameFloat(const A a, const B b) noexcept
{
    static_assert(std::is_floating_point_v<A>);
    static_assert(std::is_floating_point_v<B>);
    static_assert(std::is_same_v<std::decay_t<A>, std::decay_t<B>>);
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif
    return a == b;
#ifdef __clang__
#pragma clang diagnostic pop
#endif
}

namespace details {
template<typename T>
NODISCARD constexpr bool isBitMask()
{
    if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T>) {
        return true;
    } else if constexpr (std::is_enum_v<T>) {
        return isBitMask<std::underlying_type_t<T>>();
    } else {
        return false;
    }
}
} // namespace details

template<typename T>
NODISCARD constexpr bool isPowerOfTwo(const T x) noexcept
{
    static_assert(details::isBitMask<T>());
    if constexpr (std::is_enum_v<T>) {
        using U = std::underlying_type_t<T>;
        return isPowerOfTwo<U>(static_cast<U>(x));
    } else if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T>) {
        return x != 0u && (x & (x - 1u)) == 0u;
    } else {
        throw std::invalid_argument("x");
    }
}

template<typename T>
NODISCARD constexpr bool isAtLeastTwoBits(const T x) noexcept
{
    static_assert(details::isBitMask<T>());
    if constexpr (std::is_enum_v<T>) {
        using U = std::underlying_type_t<T>;
        return isAtLeastTwoBits<U>(static_cast<U>(x));
    } else if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T>) {
        return x != 0u && (x & (x - 1u)) != 0u;
    } else {
        throw std::invalid_argument("x");
    }
}

template<typename T>
NODISCARD bool anySet(const T src, const T mask)
{
    static_assert(details::isBitMask<T>());
    assert(isAtLeastTwoBits(mask));
    return (src & mask) != T{};
}

template<typename T>
NODISCARD bool allSet(const T src, const T mask)
{
    static_assert(details::isBitMask<T>());
    assert(isAtLeastTwoBits(mask));
    return (src & mask) == mask;
}

template<typename T>
NODISCARD bool isSet(const T src, const T bit)
{
    static_assert(details::isBitMask<T>());
    assert(isPowerOfTwo(bit));
    // use the test for any
    return (src & bit) != T{};
}

} // namespace utils

template<typename T>
NODISCARD constexpr bool isClamped(T x, T lo, T hi)
{
    return x >= lo && x <= hi;
}

namespace utils {
NODISCARD int round_ftoi(float f);
} // namespace utils

template<typename Base, typename Derived>
NODISCARD std::unique_ptr<Base> static_upcast(std::unique_ptr<Derived> &&ptr)
{
    static_assert(std::is_base_of_v<Base, Derived>);
    return std::unique_ptr<Base>(ptr.release());
}

template<typename T>
NODISCARD inline T &deref(T *const ptr)
{
    if (ptr == nullptr) {
        throw NullPointerException();
    }
    return *ptr;
}
template<typename T>
NODISCARD inline T deref(std::optional<T> &&ptr)
{
    // note: this can throw bad_optional_access
    return std::move(ptr).value();
}
template<typename T>
NODISCARD inline T &deref(std::optional<T> &ptr)
{
    // note: this can throw bad_optional_access
    return ptr.value();
}
template<typename T>
NODISCARD inline const T &deref(const std::optional<T> &ptr)
{
    // note: this can throw bad_optional_access
    return ptr.value();
}

// Technically we could make this return move or copy of the pointed-to value,
// but that's probably not what the caller expects.
template<typename T>
inline T deref(std::shared_ptr<T> &&ptr) = delete;
template<typename T>
NODISCARD inline T &deref(const std::shared_ptr<T> &ptr)
{
    if (ptr == nullptr) {
        throw NullPointerException();
    }
    return *ptr;
}
// Technically we could make this return move or copy of the pointed-to value,
// but that's probably not what the caller expects.
template<typename T>
inline T deref(std::unique_ptr<T> &&ptr) = delete;
template<typename T>
NODISCARD inline T &deref(const std::unique_ptr<T> &ptr)
{
    if (ptr == nullptr) {
        throw NullPointerException();
    }
    return *ptr;
}
template<typename T>
NODISCARD inline T &deref(const QPointer<T> &ptr)
{
    if (ptr == nullptr) {
        throw NullPointerException();
    }
    return *ptr;
}

///  Can throw NullPointerException or std::bad_cast
template<typename /* must be specified */ Derived, typename /* deduced */ Base>
NODISCARD Derived checked_dynamic_downcast(Base ptr) CAN_THROW
{
    static_assert(std::is_same_v<Base, std::remove_reference_t<Base>>);
    static_assert(std::is_same_v<Derived, std::remove_reference_t<Derived>>);
    static_assert(std::is_pointer_v<Base>);
    static_assert(std::is_pointer_v<Derived>);
    static_assert(!std::is_same_v<Base, Derived>);

    using actual_base = std::remove_pointer_t<Base>;
    using actual_derived = std::remove_pointer_t<Derived>;

    static_assert(!std::is_same_v<actual_base, actual_derived>);
    static_assert(std::is_base_of_v<actual_base, actual_derived>);
    static_assert(std::is_const_v<actual_base> == std::is_const_v<actual_derived>);

    // Using reference to force dynamic_cast to throw.
    return &dynamic_cast<actual_derived &>(deref(ptr));
}

///  Can throw NullPointerException
template<typename /* must be specified */ Base, typename /* deduced */ Derived>
NODISCARD Base checked_static_upcast(Derived ptr) CAN_THROW
{
    static_assert(std::is_same_v<Derived, std::remove_reference_t<Derived>>);
    static_assert(std::is_same_v<Base, std::remove_reference_t<Base>>);
    static_assert(std::is_pointer_v<Derived>);
    static_assert(std::is_pointer_v<Base>);
    static_assert(!std::is_same_v<Derived, Base>);

    using actual_derived = std::remove_pointer_t<Derived>;
    using actual_base = std::remove_pointer_t<Base>;

    static_assert(!std::is_same_v<actual_derived, actual_base>);
    static_assert(std::is_base_of_v<actual_base, actual_derived>);
    static_assert(std::is_const_v<actual_derived> == std::is_const_v<actual_base>);

    std::ignore = deref(ptr);
    return static_cast<Base>(ptr);
}

template<typename T>
inline auto as_unsigned_cstring(T s) = delete;

template<>
NODISCARD inline auto as_unsigned_cstring(const char *const s)
{
    return reinterpret_cast<const unsigned char *>(s);
}

template<typename T>
inline auto as_cstring(T s) = delete;

template<>
NODISCARD inline auto as_cstring(const unsigned char *const s)
{
    return reinterpret_cast<const char *>(s);
}

namespace utils {
NODISCARD std::optional<bool> getEnvBool(const char *key);
NODISCARD std::optional<int> getEnvInt(const char *key);
} // namespace utils

namespace utils {
// Use this if you're tired of having to use memcmp()
// to avoid compiler complaining about float comparison.
template<typename T>
NODISCARD constexpr bool equals(const T a, const T b)
{
    if constexpr (std::is_floating_point_v<T>) {
#ifdef _MSC_VER
#pragma warning(push, 0)
#elif defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
#endif
        return a == b;
#ifdef _MSC_VER
#pragma warning(pop)
#elif defined(__clang__)
#pragma clang diagnostic pop
#else
#pragma GCC diagnostic pop
#endif
    } else {
        return a == b;
    }
}

template<size_t N>
NODISCARD inline constexpr auto rotate_bits64(const uint64_t x) noexcept
{
    static_assert(1 <= N && N < 64);
    return (x << N) | (x >> (64 - N));
}

template<typename T>
NODISCARD static inline T clampNonNegative(T x)
{
    static_assert((std::is_integral_v<T> && std::is_signed_v<T>) || std::is_floating_point_v<T>);
    return std::max(T(0), x);
}

template<typename T>
NODISCARD static inline auto pop_back(T &container)
{
    auto x = std::move(container.back());
    container.pop_back();
    return x;
}

template<typename T>
NODISCARD static inline auto pop_front(T &container)
{
    auto x = std::move(container.front());
    container.pop_front();
    return x;
}

template<typename T>
NODISCARD static inline auto pop_front(std::queue<T> &container)
{
    auto x = std::move(container.front());
    container.pop();
    return x;
}

template<typename T>
NODISCARD static inline auto pop_top(T &container)
{
    auto x = std::move(container.top());
    container.pop();
    return x;
}

// returns the number of elements removed
template<typename Container, typename Callback>
ALLOW_DISCARD static inline size_t erase_if(Container &container, Callback &&callback)
{
    const auto beg = container.begin();
    const auto end = container.end();
    if (beg == end) {
        return 0;
    }

    const size_t before = container.size();
    const auto it = std::remove_if(beg, end, std::forward<Callback>(callback));
    if (it == end) {
        return 0;
    }

    container.erase(it, end);
    const auto after = container.size();
    assert(before > after);
    return before - after;
}

template<typename T, typename Predicate>
NODISCARD bool listRemoveIf(std::list<T> &list, Predicate &&should_remove)
{
    // c++20 version of remove_if() returns # of elements removed, but c++17 doesn't.
    bool removed = false;
    list.remove_if([&removed, &should_remove](const T &element) -> bool {
        if (should_remove(element)) {
            removed = true;
            return true;
        }
        return false;
    });
    return removed;
}

template<typename Container, typename Callback>
NODISCARD static inline auto find_min_computed(const Container &container, Callback &&callback)
{
    using OptResult = std::optional<std::decay_t<decltype(callback(*container.begin()))>>;
    if (container.empty()) {
        return OptResult{};
    }

    const auto beg = container.begin();
    const auto end = container.end();
    if ((false)) {
        auto result = callback(*beg);
        for (auto it = std::next(beg); it != end; ++it) {
            result = std::min(callback(*it), result);
        }
        return OptResult{result};
    } else {
        const auto it = std::min_element(beg,
                                         end,
                                         [&callback](const auto &a, const auto &b) -> bool {
                                             return callback(a) < callback(b);
                                         });
        return OptResult{callback(*it)};
    }
}

// This can be removed and replaced with std::remove_cvref_t<T> in c++20.
template<typename T>
using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;

template<typename T, typename... Ts>
struct are_distinct : std::conjunction<std::negation<std::is_same<T, Ts>>..., are_distinct<Ts...>>
{};

template<typename T>
struct are_distinct<T> : std::true_type
{};

template<typename... Args>
static inline constexpr bool are_distinct_v = are_distinct<Args...>::value;

} // namespace utils
