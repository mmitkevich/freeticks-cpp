#pragma once
#include "ft/utils/Common.hpp"

#include "toolbox/io/State.hpp"
#include "toolbox/io/Reactor.hpp"

namespace toolbox { inline namespace io {
    class Reactor;
}}

namespace ft::io {

// TODO: pimpl wrapper ?
using Reactor = toolbox::io::Reactor;
using State = toolbox::io::State;

Reactor& current_reactor();

}