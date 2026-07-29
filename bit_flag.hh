#pragma once

#include <cstddef>
#include <cstdint>
#include <concepts>
#include <meta>
#include <limits>
#include <utility>
#include <initializer_list>
#include <ranges>
#include <type_traits>
#include <algorithm>
#include <functional>
#include <bit>
#include <optional>

namespace mylib {

namespace details {

template<typename Enum>
inline constexpr std::size_t max_enumerator_value = [] consteval {
    auto enumerators = enumerators_of(^^Enum);
    if (enumerators.empty()) {
        return 0uz;
    }
    auto [min, max] = std::ranges::minmax(
        enumerators | std::views::transform([](auto i) { return std::to_underlying(extract<Enum>(i)); })
    );
    if constexpr (std::is_signed_v<std::underlying_type_t<Enum>>) {
        if (min < 0) {
            throw std::meta::exception("negative", ^^Enum);
        }
    }
    return std::size_t(max);
}();

template<typename Enum>
using default_representation_type = [:[] consteval {
    static constexpr std::size_t max = max_enumerator_value<Enum>;
    if (max < 8) return ^^decltype(std::uint8_t{});
    if (max < 16) return ^^decltype(std::uint16_t{});
    if (max < 32) return ^^decltype(std::uint32_t{});
    if (max < 64) return ^^decltype(std::uint64_t{});
    throw std::meta::exception("too large", ^^Enum);
}():];

} // namespace mylib::details

// Enum values are used as indices of the bit flag
template<typename Enum, typename RepType = details::default_representation_type<Enum>>
class bit_flag
{
    static_assert(std::is_enum_v<Enum>);
    static_assert(std::is_same_v<std::remove_cvref_t<RepType>, RepType>);
    static_assert(std::is_integral_v<RepType> && std::is_unsigned_v<RepType> && !std::is_same_v<RepType, bool>);
    static_assert(std::size_t(std::numeric_limits<RepType>::digits) > details::max_enumerator_value<Enum>);

public:
    using enum_type = Enum;
    using representation_type = RepType;

private:
    static constexpr representation_type zero = 0;
    static constexpr representation_type one = 1;
    static constexpr representation_type all_set = [] consteval {
        representation_type rt = 0;
        for (auto info : enumerators_of(^^enum_type)) {
            rt |= (one << std::to_underlying(extract<enum_type>(info)));
        }
        return rt;
    }();

    static consteval bit_flag all_set_flag() noexcept { return from_representation(all_set); }

    static constexpr bool is_valid_enum(enum_type e) noexcept {
        const auto val = std::to_underlying(e);
        if (val < 0 || val > details::max_enumerator_value<enum_type>) return false;
        return ((one << val) & all_set) != zero;
    }

public:
    constexpr bit_flag() = default;
    constexpr bit_flag(const bit_flag&) = default;
    constexpr bit_flag& operator=(const bit_flag&) = default;

    // Implicit conversion from enum value, unknown values are ignored
    constexpr /* implicit */ bit_flag(enum_type e) noexcept
        : bit_flag(from_representation(is_valid_enum(e) ? one << std::to_underlying(e) : zero))
    {}

    constexpr bit_flag(std::initializer_list<enum_type> il) noexcept : bit_flag(std::from_range, il) {}

    template<std::ranges::input_range R>
        requires std::same_as<enum_type, std::ranges::range_value_t<R>>
    constexpr bit_flag(std::from_range_t, R&& r) {
        for (enum_type e : std::forward<R>(r)) {
            set(e);
        }
    }

    constexpr void swap(bit_flag& other) noexcept { std::ranges::swap(rep, other.rep); }
    friend constexpr void swap(bit_flag& lhs, bit_flag& rhs) noexcept { lhs.swap(rhs); }

    // Default: ignore unknown bits
    [[nodiscard]] static constexpr bit_flag from_representation(representation_type rep) noexcept {
        bit_flag b;
        b.rep = rep & all_set;
        return b;
    }

    // Unchecked
    [[nodiscard]] static constexpr bit_flag from_representation_unchecked(representation_type rep)
        pre ((rep & ~all_set) == zero)
    {
        return std::bit_cast<bit_flag>(rep);
    }

    // Unchecked
    [[nodiscard]] static constexpr bit_flag from_enum_unchecked(enum_type e)
        pre (is_valid_enum(e))
    {
        return from_representation_unchecked(one << std::to_underlying(e));
    }

    // Checked
    [[nodiscard]] static constexpr std::optional<bit_flag> try_from_representation(representation_type rep) noexcept {
        return (rep & ~all_set) == zero ? std::make_optional(from_representation_unchecked(rep)) : std::nullopt;
    }

    // Checked
    [[nodiscard]] static constexpr std::optional<bit_flag> try_from_enum(enum_type e) noexcept {
        return is_valid_enum(e) ? std::make_optional(from_enum_unchecked(e)) : std::nullopt;
    }

    [[nodiscard]] constexpr representation_type to_representation() const noexcept { return rep; }

    constexpr bit_flag& set(bit_flag other, bool value = true) noexcept { return value ? (rep |= other.rep, *this) : reset(other); }
    constexpr bit_flag& set() noexcept { return set(all_set_flag()); }
    constexpr bit_flag& reset(bit_flag other) noexcept { rep &= ~other.rep; return *this; }
    constexpr bit_flag& reset() noexcept { return reset(all_set_flag()); }
    constexpr bit_flag& flip(bit_flag other) noexcept { rep ^= other.rep; return *this; }
    constexpr bit_flag& flip() noexcept { return flip(all_set_flag()); }

    [[nodiscard]] constexpr bool all_of(this bit_flag self, bit_flag other) noexcept { return (self.rep & other.rep) == other.rep; }
    [[nodiscard]] constexpr bool any_of(this bit_flag self, bit_flag other) noexcept { return (self.rep & other.rep) != zero; }
    [[nodiscard]] constexpr bool none_of(this bit_flag self, bit_flag other) noexcept { return (self.rep & other.rep) == zero; }
    [[nodiscard]] constexpr bool all() const noexcept { return all_of(all_set_flag()); }
    [[nodiscard]] constexpr bool any() const noexcept { return any_of(all_set_flag()); }
    [[nodiscard]] constexpr bool none() const noexcept { return none_of(all_set_flag()); }
    [[nodiscard]] constexpr bool test(enum_type e) const noexcept { return any_of(e); }
    [[nodiscard]] constexpr std::size_t count() const noexcept { return std::popcount(rep); }

    constexpr bit_flag& operator|=(bit_flag other) noexcept { rep |= other.rep; return *this; }
    constexpr bit_flag& operator&=(bit_flag other) noexcept { rep &= other.rep; return *this; }
    constexpr bit_flag& operator^=(bit_flag other) noexcept { rep ^= other.rep; return *this; }

    [[nodiscard]] friend constexpr bit_flag operator|(bit_flag lhs, bit_flag rhs) noexcept { return lhs |= rhs; }
    [[nodiscard]] friend constexpr bit_flag operator&(bit_flag lhs, bit_flag rhs) noexcept { return lhs &= rhs; }
    [[nodiscard]] friend constexpr bit_flag operator^(bit_flag lhs, bit_flag rhs) noexcept { return lhs ^= rhs; }
    [[nodiscard]] friend constexpr bit_flag operator~(bit_flag f) noexcept { return f.flip(); }

    [[nodiscard]] friend constexpr bool operator==(bit_flag lhs, bit_flag rhs) = default;

    class reference
    {
    public:
        constexpr reference(const reference&) = default;

        constexpr const reference& operator=(reference other) const noexcept {
            return *this = static_cast<bool>(other);
        }

        constexpr const reference& operator=(bool value) const noexcept {
            parent->set(mask, value);
            return *this;
        }

        [[nodiscard]] constexpr operator bool() const noexcept { return parent->any_of(mask); }
        [[nodiscard]] constexpr bool operator~() const noexcept { return parent->none_of(mask); }
        constexpr const reference& flip() const noexcept { parent->flip(mask); return *this; }

        friend constexpr void swap(reference lhs, reference rhs) noexcept {
            bool tmp = static_cast<bool>(lhs);
            lhs = static_cast<bool>(rhs);
            rhs = tmp;
        }

        friend constexpr void swap(reference lhs, bool& rhs) noexcept {
            bool tmp = static_cast<bool>(lhs);
            lhs = static_cast<bool>(rhs);
            rhs = tmp;
        }

        friend constexpr void swap(bool& lhs, reference rhs) noexcept {
            bool tmp = static_cast<bool>(lhs);
            lhs = static_cast<bool>(rhs);
            rhs = tmp;
        }

    private:
        friend class bit_flag;
        explicit constexpr reference(bit_flag* parent, enum_type e) noexcept : parent(parent), mask(e) {}

        bit_flag* parent;
        bit_flag mask;
    };

    [[nodiscard]] constexpr reference operator[](enum_type e) noexcept { return reference(this, e); }
    [[nodiscard]] constexpr bool operator[](enum_type e) const noexcept { return test(e); }

private:
    representation_type rep = zero;
};

template<std::ranges::input_range R>
    requires std::is_enum_v<std::ranges::range_value_t<R>>
bit_flag(std::from_range_t, R&& r) -> bit_flag<std::ranges::range_value_t<R>>;

namespace details {

template<typename T>
concept is_bit_flag_reference = [] consteval {
    static constexpr auto ref_info = ^^T;
    static constexpr auto parent = [] consteval {
        if (!has_parent(ref_info)) return std::meta::info{};
        auto parent = parent_of(ref_info);
        if (!is_class_type(parent)) return std::meta::info{};
        if (!(has_template_arguments(parent) && template_of(parent) == ^^mylib::bit_flag)) return std::meta::info{};
        return parent;
    }();
    if constexpr (parent == std::meta::info{}) {
        return false;
    } else {
        return is_same_type(^^typename [:parent:]::reference, ref_info);
    }
}();

} // namespace mylib::details

} // namespace mylib

template<typename Enum, typename RepType>
struct std::hash<mylib::bit_flag<Enum, RepType>> {
    constexpr std::size_t operator()(mylib::bit_flag<Enum, RepType> b) const noexcept {
        return std::hash<RepType>{}(b.to_representation());
    }
};

template<mylib::details::is_bit_flag_reference BitFlagRef>
struct std::hash<BitFlagRef> {
    constexpr std::size_t operator()(BitFlagRef ref) const noexcept {
        return std::hash<bool>{}(static_cast<bool>(ref));
    }
};
