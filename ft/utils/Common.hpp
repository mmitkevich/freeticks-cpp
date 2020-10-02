#pragma once
#include <cstdint>
#include <string_view>
#include <string>
#include <iostream>
#include <cassert>
#include <sstream>
#include <utility>

#include <boost/mp11/list.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/tuple.hpp>

#include "toolbox/sys/Log.hpp"

#include "Trace.hpp"
#include "Signal.hpp"

namespace mp = boost::mp11;

namespace ftu = ft::utils;

namespace tb = toolbox;
namespace tbu = toolbox::util;