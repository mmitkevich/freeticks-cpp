#include "Service.hpp"


namespace ft { namespace io {

static /*thread_local*/ io::Reactor g_reactor; // FIXME: reactor should be threadlocal

io::Reactor* current_reactor() {
    return &g_reactor;
}

}} // ft::core