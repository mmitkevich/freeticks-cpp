#pragma once

#include "toolbox/io/Reactor.hpp"
#include "toolbox/io/Runner.hpp"

namespace ft::io {

using Reactor = toolbox::io::Reactor;

Reactor* current_reactor();


template<typename ReactorT>
class BasicService {
public:
    using Reactor = ReactorT;
public:
    BasicService(Reactor* reactor)
    : reactor_(reactor) {}

    // Copy
    BasicService(const BasicService&) = delete;
    BasicService&operator=(const BasicService&) = delete;

    // Move
    BasicService(BasicService&&) = default;
    BasicService& operator=(BasicService&&) = default;

    void open() {}
    void close() {}

    Reactor& reactor() { return *reactor_; }
protected:
    Reactor* reactor_{};
};

}