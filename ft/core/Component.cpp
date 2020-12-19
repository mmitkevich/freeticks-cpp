#include "Component.hpp"
#include "toolbox/io/Reactor.hpp"
#include "toolbox/io/Runner.hpp"

namespace ft { inline namespace core {

static Reactor g_reactor;

Reactor* current_reactor() {
    return &g_reactor;
}

}} // ft::core