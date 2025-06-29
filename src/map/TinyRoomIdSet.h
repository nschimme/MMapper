#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2021 The MMapper Authors

#include "RoomIdSet.h"
#include "roomid.h"

#include <cassert>
#include <memory>
#include <new>
#include <optional>
#include <set>
#include <utility>
#include <variant>

template<typename Type_, typename Set_, typename IdHelper_>
struct NODISCARD TinySet
{
private:
    using Type = Type_;
    using BigSet = Set_; // big as opposed to tiny
    using SharedConstBigSet = std::shared_ptr<const BigSet>;

private:
    struct NODISCARD Variant final
    {
    public:
        struct EmptyTag { // Tag for empty state
            // Two EmptyTags are always considered equal
            bool operator==(const EmptyTag& /*other*/) const noexcept {
                return true;
            }
        };

    private:
        std::variant<EmptyTag, Type, SharedConstBigSet> m_data;

    public:
        Variant() : m_data(EmptyTag{}) {} // Default construct to EmptyTag

        // Note: Custom operator== for Variant might be tricky if BigSet itself doesn't have a cheap operator==
        // or if direct comparison of SharedConstBigSet (pointer comparison) is intended for some scenarios.
        // For now, rely on std::variant's operator== if Type and SharedConstBigSet are comparable.
        // If specific logic like pointer comparison for SharedConstBigSet was intended by the old memcmp,
        // this needs careful thought. Assuming content comparison for BigSet is desired if both are big.
        // std::variant::operator== will do the right thing if alternatives are comparable.
        // However, SharedConstBigSet (shared_ptr) comparison compares pointers, not pointees by default.
        // This might be an issue for TinySet::operator== later.
        NODISCARD bool operator==(const Variant &other) const noexcept
        {
            // If both hold SharedConstBigSet, we need to compare the sets themselves, not just the shared_ptrs.
            if (std::holds_alternative<SharedConstBigSet>(m_data) && std::holds_alternative<SharedConstBigSet>(other.m_data)) {
                const auto& ptr1 = std::get<SharedConstBigSet>(m_data);
                const auto& ptr2 = std::get<SharedConstBigSet>(other.m_data);
                if (ptr1 == ptr2) return true; // Same pointer or both null
                if (ptr1 && ptr2) return *ptr1 == *ptr2; // Compare pointed-to sets
                return false; // One null, one not
            }
            // For other types (EmptyTag, Type), std::variant's default comparison is fine.
            return m_data == other.m_data;
        }


    public:
        NODISCARD bool holds_nothing() const noexcept { return std::holds_alternative<EmptyTag>(m_data); }
        NODISCARD bool holds_single_value() const noexcept { return std::holds_alternative<Type>(m_data); }
        NODISCARD bool holds_shared_const_bigset() const noexcept { return std::holds_alternative<SharedConstBigSet>(m_data); }

    public:
        void clear() noexcept
        {
            m_data.template emplace<EmptyTag>();
            assert(holds_nothing());
        }

    public:
        NODISCARD Type get_single_value() const
        {
            assert(holds_single_value());
            return std::get<Type>(m_data);
        }
        void set_single_value(const Type value)
        {
            m_data = value;
            assert(holds_single_value());
        }

    public:
        // Added for use_count checks
        NODISCARD SharedConstBigSet get_const_big_set_ptr() const
        {
            assert(holds_shared_const_bigset());
            return std::get<SharedConstBigSet>(m_data);
        }

        NODISCARD const BigSet &get_big() const
        {
            assert(holds_shared_const_bigset());
            // get_const_big_set_ptr() returns a shared_ptr, dereference it.
            // Ensure the shared_ptr is not null before dereferencing,
            // though holds_shared_const_bigset should imply it's a valid set.
            // A null shared_ptr in the variant when holds_shared_const_bigset() is true
            // would signify an empty big set, which should ideally transition to EmptyTag.
            const auto& ptr = std::get<SharedConstBigSet>(m_data);
            if (!ptr) {
                 // This state (holds_shared_const_bigset is true but ptr is null) should ideally be avoided.
                 // If it means an empty set, it should have been transitioned to EmptyTag or handled.
                 // For now, crashing is safer than returning a reference to garbage.
                 // This indicates an issue with how null (empty) BigSets are being assigned.
                throw std::logic_error("Null BigSet pointer in Variant despite holding SharedConstBigSet type");
            }
            return *ptr;
        }

        // newValue can be nullptr (representing an empty big set, which should then transition to EmptyTag)
        void assign_shared_const_bigset(SharedConstBigSet newValue)
        {
            if (!newValue) { // If assigning a null shared_ptr, it means the set is empty.
                m_data.template emplace<EmptyTag>();
                assert(holds_nothing());
            } else {
                m_data = std::move(newValue);
                assert(holds_shared_const_bigset());
            }
        }
    };
    Variant m_var;

private:
    NODISCARD bool isEmpty() const noexcept { return m_var.holds_nothing(); }
    NODISCARD bool hasOne() const noexcept { return m_var.holds_single_value(); }
    NODISCARD bool hasBig() const noexcept { return m_var.holds_shared_const_bigset(); }

private:
    NODISCARD const BigSet &getBig() const
    {
        if (!hasBig()) { // Should be checked by caller
            throw std::logic_error("getBig() called on non-big set");
        }
        return m_var.get_big();
    }
    NODISCARD Type getOnly() const
    {
        if (!hasOne()) { // Should be checked by caller
            throw std::logic_error("getOnly() called on non-single set");
        }
        return m_var.get_single_value();
    }

private:
    // Assigns a shared pointer to a potentially new BigSet.
    // This is used when a BigSet is newly created or copied for modification.
    void assign(SharedConstBigSet new_shared_set) // new_shared_set can be nullptr
    {
        // m_var.assign_shared_const_bigset handles the logic of emplacing EmptyTag if new_shared_set is null
        m_var.assign_shared_const_bigset(std::move(new_shared_set));

        if (!new_shared_set) {
            assert(isEmpty());
        } else {
            assert(hasBig());
        }
    }

    void assign(Type value) noexcept
    {
        m_var.set_single_value(value);
        assert(hasOne());
    }

    void clear() noexcept
    {
        m_var.clear();
        assert(isEmpty());
    }

public:
    TinySet() noexcept = default; // m_var default constructor initializes to EmptyTag
    explicit TinySet(const Type one) // Changed RoomId to Type for generality
    {
        // m_var is already empty by default construction.
        assign(one);
        assert(hasOne());
    }

    ~TinySet() = default; // std::variant handles cleanup of its alternatives.

    // Move constructor: std::variant is movable.
    TinySet(TinySet &&other) noexcept = default;

    // Copy constructor
    TinySet(const TinySet &other)
    {
        if (other.hasBig()) {
            // Share ownership of the const BigSet by copying the shared_ptr
            assign(other.m_var.get_const_big_set_ptr());
        } else if (other.hasOne()) {
            assign(other.getOnly());
        } else { // other is empty
            // Default constructed m_var is already empty. Or if *this was not empty:
            clear();
        }
    }

    // Move assignment: std::variant is move-assignable.
    TinySet &operator=(TinySet &&other) noexcept = default;

    // Copy assignment
    TinySet &operator=(const TinySet &other)
    {
        if (std::addressof(other) == this) {
            return *this;
        }
        if (other.hasBig()) {
            assign(other.m_var.get_const_big_set_ptr());
        } else if (other.hasOne()) {
            assign(other.getOnly());
        } else { // other is empty
            clear();
        }
        return *this;
    }

    explicit TinySet(BigSet &&other) = delete; // Keep deleted

public:
    struct NODISCARD ConstIterator final
    {
    private:
        using SetConstIt = typename BigSet::ConstIterator;
        SetConstIt m_setIt{};
        Type m_val{};
        enum class NODISCARD StateEnum : uint8_t { Empty, One, Big };
        StateEnum m_state = StateEnum::Empty;

    private:
        friend TinySet;
        explicit ConstIterator() = default;
        explicit ConstIterator(const Type val)
            : m_val{val}
            , m_state{StateEnum::One}
        {}
        explicit ConstIterator(const SetConstIt setIt)
            : m_setIt{setIt}
            , m_state{StateEnum::Big}
        {}

    public:
        NODISCARD Type operator*() const
        {
            switch (m_state) {
            case StateEnum::Empty:
                assert(false);
                return Type{};
            case StateEnum::One:
                return m_val;
            case StateEnum::Big:
                return *m_setIt;
            }
            assert(false);
            std::abort();
            return Type{};
        }
        ALLOW_DISCARD ConstIterator &operator++()
        {
            switch (m_state) {
            case StateEnum::Empty:
                assert(false);
                break;
            case StateEnum::One:
                m_state = StateEnum::Empty;
                break;
            case StateEnum::Big:
                ++m_setIt;
                break;
            }
            return *this;
        }
        void operator++(int) = delete;
        NODISCARD bool operator==(const ConstIterator &other) const
        {
            if (other.m_state != m_state) {
                return false;
            }

            switch (m_state) {
            case StateEnum::Empty:
                return true;
            case StateEnum::One:
                return m_val == other.m_val;
            case StateEnum::Big:
                return m_setIt == other.m_setIt;
            }
            assert(false);
            std::abort();
            return false;
        }
        NODISCARD bool operator!=(const ConstIterator &other) const { return !operator==(other); }
    };

public:
    NODISCARD ConstIterator begin() const
    {
        if (isEmpty()) {
            return ConstIterator{};
        } else if (hasOne()) {
            return ConstIterator{getOnly()};
        } else {
            return ConstIterator{getBig().begin()};
        }
    }
    NODISCARD ConstIterator end() const
    {
        if (isEmpty() || hasOne()) {
            return ConstIterator{};
        } else {
            return ConstIterator{getBig().end()};
        }
    }
    NODISCARD size_t size() const
    {
        if (isEmpty()) {
            return 0;
        } else if (hasOne()) {
            return 1;
        } else {
            return getBig().size();
        }
    }

public:
    NODISCARD bool empty() const noexcept { return isEmpty(); }
    NODISCARD bool contains(const Type id) const
    {
        if (isEmpty()) {
            return false;
        } else if (hasOne()) {
            return getOnly() == id;
        } else {
            // getBig() returns const BigSet&, which is fine for const operations
            return getBig().contains(id);
        }
    }

    NODISCARD bool operator==(const TinySet &rhs) const
    {
        // Relies on Variant::operator== which handles the comparison of underlying data,
        // including special logic for comparing BigSet content if both are SharedConstBigSet.
        return m_var == rhs.m_var;
    }

    NODISCARD bool operator!=(const TinySet &rhs) const { return !operator==(rhs); }

public:
    void erase(const Type id)
    {
        assert(!IdHelper_::isInvalid(id));

        if (hasOne()) {
            if (getOnly() == id) {
                clear(); // Transitions to EmptyTag
            }
            // If not equal, or if it was already empty, do nothing.
        } else if (hasBig()) {
            SharedConstBigSet current_big_set_ptr = m_var.get_const_big_set_ptr();
            if (!current_big_set_ptr) { /* Should not happen if hasBig() is true */ return; }

            // Check if id is actually in the set. If not, no modification needed.
            if (!current_big_set_ptr->contains(id)) {
                return;
            }

            // Perform copy-on-write: create a mutable copy
            auto mutable_big_set = std::make_shared<BigSet>(*current_big_set_ptr);
            mutable_big_set->erase(id);

            if (mutable_big_set->empty()) {
                assign(nullptr); // assign(nullptr) will call m_var.assign_shared_const_bigset(nullptr) which sets to EmptyTag
                assert(isEmpty());
            } else if (mutable_big_set->size() == 1) {
                assign(*mutable_big_set->begin()); // assign(Type) will call m_var.set_single_value()
                assert(hasOne());
            } else {
                // assign(SharedConstBigSet) will call m_var.assign_shared_const_bigset()
                assign(std::const_pointer_cast<const BigSet>(mutable_big_set));
                assert(hasBig());
            }
        }
        // If isEmpty(), do nothing.
    }

    void insert(const Type id)
    {
        assert(!IdHelper_::isInvalid(id));

        if (contains(id)) { // contains() is const, should correctly use new Variant
            return;
        }

        if (isEmpty()) {
            assign(id); // assign(Type) -> m_var.set_single_value()
            assert(hasOne());
        } else if (hasOne()) {
            Type current_val = getOnly(); // getOnly() -> m_var.get_single_value()
            // Convert to BigSet
            auto new_big_set = std::make_shared<BigSet>();
            new_big_set->insert(current_val);
            new_big_set->insert(id);
            // assign(SharedConstBigSet) -> m_var.assign_shared_const_bigset()
            assign(std::const_pointer_cast<const BigSet>(new_big_set));
            assert(hasBig());
        } else { // hasBig()
            SharedConstBigSet current_big_set_ptr = m_var.get_const_big_set_ptr();
            if (!current_big_set_ptr) { /* Should not happen if hasBig() is true */ return; }

            // Copy-on-write: create a mutable copy
            auto new_big_set = std::make_shared<BigSet>(*current_big_set_ptr);
            new_big_set->insert(id);
            // assign(SharedConstBigSet) -> m_var.assign_shared_const_bigset()
            assign(std::const_pointer_cast<const BigSet>(new_big_set));
            assert(hasBig());
        }
    }

public:
    void insertAll(const TinySet &other)
    {
        for (const Type x : other) {
            insert(x);
        }
    }

public:
    NODISCARD Type first() const
    {
        if (isEmpty()) {
            throw std::out_of_range("set is empty");
        } else if (hasOne()) {
            return getOnly();
        } else {
            return *getBig().begin();
        }
    }
};

namespace detail {

template<typename Tag_>
struct NODISCARD InvalidHelper;

template<>
struct NODISCARD InvalidHelper<RoomId> final
{
    static constexpr const RoomId INVALID = INVALID_ROOMID;
};

template<>
struct NODISCARD InvalidHelper<ExternalRoomId> final
{
    static constexpr const ExternalRoomId INVALID = INVALID_EXTERNAL_ROOMID;
};

template<typename IdType_>
struct NODISCARD RoomIdHelper final
{
    using IdType = IdType_;
    NODISCARD static bool isInvalid(const IdType id)
    {
        return id == InvalidHelper<IdType>::INVALID;
    }
    NODISCARD static size_t getBits(const IdType id) { return static_cast<size_t>(id.value()); }
    NODISCARD static IdType fromBits(const size_t bits)
    {
        return IdType{static_cast<uint32_t>(bits)};
    }
};

template<typename IdType_>
using TinyHelper = TinySet<IdType_, BasicRoomIdSet<IdType_>, RoomIdHelper<IdType_>>;

} // namespace detail

using TinyRoomIdSet = detail::TinyHelper<RoomId>;
using TinyExternalRoomIdSet = detail::TinyHelper<ExternalRoomId>;

NODISCARD extern RoomIdSet toRoomIdSet(const TinyRoomIdSet &set);
NODISCARD extern TinyRoomIdSet toTinyRoomIdSet(const RoomIdSet &set);

namespace test {
extern void testTinyRoomIdSet();
} // namespace test
