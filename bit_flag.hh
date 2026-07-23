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

namespace mylib {

namespace details {

template<typename Enum>
consteval std::size_t max_enumerator_value() {
    auto enumerators = enumerators_of(^^Enum);
    if (enumerators.empty()) {
        throw std::meta::exception("no enumerators", ^^Enum);
    }
    auto [min, max] = std::ranges::minmax(enumerators
        | std::views::transform([](auto i) { return std::to_underlying(extract<Enum>(i)); })
    );
    if constexpr (std::is_signed_v<std::underlying_type_t<Enum>>) {
        if (min < 0) {
            throw std::meta::exception("negative", ^^Enum);
        }
    }
    return max;
}

template<typename Enum>
using default_representation_type = [:[] consteval {
    static constexpr std::size_t max = max_enumerator_value<Enum>();
    if (max < 8) return ^^decltype(std::uint8_t{});
    if (max < 16) return ^^decltype(std::uint16_t{});
    if (max < 32) return ^^decltype(std::uint32_t{});
    if (max < 64) return ^^decltype(std::uint64_t{});
    throw std::meta::exception("too large", ^^Enum);
}():];

} // namespace mylib::details

// Enum values are used as indices of the bit flag.
// Enum values not presented in definition are ignored.
template<typename Enum, typename RepType = details::default_representation_type<Enum>>
class bit_flag
{
    static_assert(std::is_enum_v<Enum>);
    static_assert(std::is_integral_v<RepType> && std::is_unsigned_v<RepType> && !std::is_same_v<RepType, bool>);
    static_assert(std::size_t(std::numeric_limits<RepType>::digits) > details::max_enumerator_value<Enum>());

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
    static constexpr bit_flag all_set_flag = from_representation(all_set);

    static constexpr bool filter_unchecked_enum(enum_type e) noexcept {
        auto val = std::to_underlying(e);
        if constexpr (std::is_signed_v<decltype(val)>) {
            if (val < 0) {
                return false;
            }
        }
        return val <= details::max_enumerator_value<enum_type>();
    }

public:
    constexpr bit_flag() = default;
    constexpr bit_flag(const bit_flag&) = default;
    constexpr bit_flag& operator=(const bit_flag&) = default;

    constexpr bit_flag(enum_type e) noexcept
        : bit_flag(from_representation(filter_unchecked_enum(e) ? one << std::to_underlying(e) : zero))
    {}

    constexpr bit_flag(std::initializer_list<enum_type> il) noexcept : bit_flag(std::from_range, il) {}

    template<std::ranges::input_range R>
        requires std::same_as<enum_type, std::ranges::range_value_t<R>>
    constexpr bit_flag(std::from_range_t, R&& r) {
        for (enum_type e : std::forward<R>(r)) {
            set(e);
        }
    }

    [[nodiscard]] static constexpr bit_flag from_representation(representation_type rep) noexcept {
        bit_flag b;
        b.rep = rep & all_set;
        return b;
    }

    [[nodiscard]] constexpr representation_type to_representation() const noexcept { return rep; }

    constexpr bit_flag& set(bit_flag other) noexcept { rep |= other.rep; return *this; }
    constexpr bit_flag& set() noexcept { return set(all_set_flag); }
    constexpr bit_flag& reset(bit_flag other) noexcept { rep &= ~other.rep; return *this; }
    constexpr bit_flag& reset() noexcept { return reset(all_set_flag); }
    constexpr bit_flag& flip(bit_flag other) noexcept { rep ^= other.rep; return *this; }
    constexpr bit_flag& flip() noexcept { return flip(all_set_flag); }
    [[nodiscard]] constexpr bool all_of(bit_flag other) const noexcept { return (rep & other.rep) == other.rep; }
    [[nodiscard]] constexpr bool any_of(bit_flag other) const noexcept { return (rep & other.rep) != zero; }
    [[nodiscard]] constexpr bool none_of(bit_flag other) const noexcept { return (rep & other.rep) == zero; }
    [[nodiscard]] constexpr bool all() const noexcept { return all_of(all_set_flag); }
    [[nodiscard]] constexpr bool any() const noexcept { return any_of(all_set_flag); }
    [[nodiscard]] constexpr bool none() const noexcept { return none_of(all_set_flag); }
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

private:
    representation_type rep = zero;
};

template<std::ranges::input_range R>
    requires std::is_enum_v<std::ranges::range_value_t<R>>
bit_flag(std::from_range_t, R&& r) -> bit_flag<std::ranges::range_value_t<R>>;

} // namespace mylib

template<typename Enum, typename RepType>
struct std::hash<mylib::bit_flag<Enum, RepType>> {
    constexpr std::size_t operator()(mylib::bit_flag<Enum, RepType> b) const noexcept {
        return std::hash<RepType>{}(b.to_representation());
    }
};
