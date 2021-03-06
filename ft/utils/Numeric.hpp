#pragma once

#include "ft/utils/Common.hpp"

namespace ft{ inline namespace util {

template<typename T> 
struct DoubleConversion {
    template<typename...ArgsT>
    constexpr double to_double(T arg, ArgsT...) { return (double) arg;}
    template<typename...ArgsT>
    constexpr T from_double(double arg, ArgsT...) { return (T) arg; }
};

template<> 
struct DoubleConversion<std::int64_t> {
    std::int64_t multiplier;
    
    constexpr DoubleConversion(std::int64_t multiplier)
    : multiplier(multiplier) {}
    
    template<typename...ArgsT>
    constexpr double to_double(std::int64_t val, ArgsT...args) const { return ((double)val * ... * args)/multiplier; }
    template<typename...ArgsT>
    std::int64_t from_double(double val, ArgsT...args) { return std::round((val/.../args)*multiplier); }
};

template<typename T> 
struct NumericComparison {
    constexpr bool eq(T lhs, T rhs) { return lhs==rhs; }
    constexpr bool lt(T lhs, T rhs) { return lhs<rhs; }
    constexpr bool gt(T lhs, T rhs) { return lhs>rhs; }
    constexpr bool le(T lhs, T rhs) { return lhs<=rhs; }
    constexpr bool ge(T lhs, T rhs) { return lhs>=rhs; }
};

template<typename VenueQtyT, std::int64_t VenueMultiplier, typename CoreQtyT, std::int64_t CoreMultiplier>
struct NumericConversion {
    constexpr CoreQtyT to_core(VenueQtyT venue_qty) { return venue_qty * CoreMultiplier / VenueMultiplier; }
    constexpr VenueQtyT from_core(CoreQtyT core_qty) { return core_qty * VenueMultiplier / CoreMultiplier; }
};

}} // ft::util