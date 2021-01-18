#pragma once
#include <cstdint>
#include <string_view>
#include <string>
#include <iostream>
#include <cassert>
#include <sstream>
#include <utility>
#include <memory>
#include <atomic>

#include <boost/mp11/list.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/tuple.hpp>

#include "toolbox/Config.h"
#include "toolbox/sys/Log.hpp"
#include "toolbox/util/Enum.hpp"
#include "toolbox/util/RobinHood.hpp"

#include "Trace.hpp"
#include "Signal.hpp"

#define FT_NO_INLINE __attribute__((noinline)) 
#define FT_ALWAYS_INLINE __attribute__((always_inline))
#define FT_LIKELY(x) (x)
#define FT_UNLIKELY(x) (x)

/// CRTP boilerplate
#define FT_MIXIN(Self) \
    constexpr Self* self() { return static_cast<Self*>(this); } \
    constexpr const Self* self() const { return static_cast<const Self*>(this); } 

#define FT_CRTP(T, B) \
  using Self = T; \
  using Base = B; \
  FT_MIXIN(Self)

namespace toolbox {
}

//namespace tb = toolbox;

namespace mp = boost::mp11;

namespace ft { inline namespace util {
        template<typename KeyT, typename ValueT, typename HashT = toolbox::util::RobinHash<KeyT>,
          typename KeyEqualT = std::equal_to<KeyT>, std::size_t MaxLoadFactor100N = 80>
        using unordered_map = toolbox::util::unordered_map<KeyT,ValueT,HashT,KeyEqualT,MaxLoadFactor100N>;
}} // ft::util