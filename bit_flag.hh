#pragma once

#include <concepts>
#include <cstdint>
#include <limits>
#include <meta>
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
    if constexpr (std::is_signed_v<Enum>) {
        if (min < 0) {
            throw std::meta::exception("negative", ^^Enum);
        }
    }
    return max;
}

template<typename Enum>
using default_underlying_type = [:[] consteval {
    static constexpr std::size_t max = max_enumerator_value<Enum>();
    if (max < 8) return ^^decltype(std::uint8_t{});
    if (max < 16) return ^^decltype(std::uint16_t{});
    if (max < 32) return ^^decltype(std::uint32_t{});
    if (max < 64) return ^^decltype(std::uint64_t{});
    throw std::meta::exception("too large", ^^Enum);
}():];

} // namespace mylib::details

template<typename Enum, typename UnderlyingType = details::default_underlying_type<Enum>>
class bit_flag
{
    static_assert(std::is_enum_v<Enum>);
    static_assert(std::is_unsigned_v<UnderlyingType>);
    static_assert(std::numeric_limits<UnderlyingType>::digits > details::max_enumerator_value<Enum>());

public:
    using enum_type = Enum;
    using underlying_type = UnderlyingType;

private:
    static constexpr underlying_type zero = 0;
    static constexpr underlying_type one = 1;
    static constexpr bit_flag all_set = [] consteval {
        return enumerators_of(^^enum_type)
            | std::views::transform(&std::meta::extract<enum_type>)
            | std::ranges::to<bit_flag<enum_type, underlying_type>>();
    }();

public:
    constexpr bit_flag() = default;
    constexpr bit_flag(const bit_flag&) = default;
    constexpr bit_flag& operator=(const bit_flag&) = default;
    constexpr bit_flag(enum_type e) noexcept : rep(one << std::to_underlying(e)) {}
    constexpr bit_flag(std::initializer_list<enum_type> il) noexcept : bit_flag(std::from_range, il) {}

    template<std::ranges::input_range R>
        requires std::same_as<enum_type, std::ranges::range_value_t<R>>
    constexpr bit_flag(std::from_range_t, R&& r) {
        for (enum_type e : std::forward<R>(r)) {
            set(e);
        }
    }

    static constexpr bit_flag from_underlying(underlying_type rep) noexcept {
        bit_flag b;
        b.rep = rep;
        return b;
    }

    constexpr underlying_type to_underlying() const noexcept { return rep; }

    constexpr bit_flag& set(bit_flag other) noexcept { rep |= other.rep; return *this; }
    constexpr bit_flag& set() noexcept { return set(all_set); }
    constexpr bit_flag& reset(bit_flag other) noexcept { rep &= ~other.rep; return *this; }
    constexpr bit_flag& reset() noexcept { return reset(all_set); }
    constexpr bit_flag& flip(bit_flag other) noexcept { rep ^= other.rep; return *this; }
    constexpr bit_flag& flip() noexcept { return flip(all_set); }
    constexpr bool all_of(bit_flag other) const noexcept { return (rep & other.rep) == other.rep; }
    constexpr bool any_of(bit_flag other) const noexcept { return (rep & other.rep) != zero; }
    constexpr bool none_of(bit_flag other) const noexcept { return (rep & other.rep) == zero; }
    constexpr bool all() const noexcept { return all_of(all_set); }
    constexpr bool any() const noexcept { return any_of(all_set); }
    constexpr bool none() const noexcept { return none_of(all_set); }
    constexpr bool test(enum_type e) const noexcept { return any_of(e); }
    constexpr std::size_t count() const noexcept { return std::popcount(rep); }

    friend constexpr bool operator==(bit_flag lhs, bit_flag rhs) = default;

private:
    underlying_type rep = zero;
};

template<std::ranges::input_range R>
    requires std::is_enum_v<std::ranges::range_value_t<R>>
bit_flag(std::from_range_t, R&& r) -> bit_flag<std::ranges::range_value_t<R>>;

} // namespace mylib

template<typename Enum, typename UnderlyingType>
struct std::hash<mylib::bit_flag<Enum, UnderlyingType>> {
    static std::size_t operator()(mylib::bit_flag<Enum, UnderlyingType> b) noexcept {
        return std::hash<UnderlyingType>{}(b.to_underlying());
    }
};
