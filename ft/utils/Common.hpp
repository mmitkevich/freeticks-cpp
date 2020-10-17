#pragma once
#include <cstdint>
#include <string_view>
#include <string>
#include <iostream>
#include <cassert>
#include <sstream>
#include <utility>
#include <memory>

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


namespace toolbox {
    inline namespace util{}
    inline namespace net{}
}

namespace tb = toolbox;

namespace mp = boost::mp11;
namespace ftu = ft::utils;
namespace tbu = tb::util;
namespace tbn = tb::net;

namespace ft {
    namespace utils {
        template<typename KeyT, typename ValueT, typename HashT = toolbox::util::RobinHash<KeyT>,
          typename KeyEqualT = std::equal_to<KeyT>, std::size_t MaxLoadFactor100N = 80>
        using FlatMap = toolbox::util::RobinFlatMap<KeyT,ValueT,HashT,KeyEqualT,MaxLoadFactor100N>;
    }
}