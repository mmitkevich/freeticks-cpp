
#include "Executor.hpp"
#include "toolbox/io/Reactor.hpp"
#include "toolbox/io/Runner.hpp"

namespace ft::core {

static Reactor g_reactor;

Reactor& current_reactor() {
    return g_reactor;
}

} // ns ft::io