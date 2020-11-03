#pragma once
#include "ft/core/Identifiable.hpp"
#include "ft/core/Component.hpp"
#include "ft/utils/Common.hpp"

#include "toolbox/io/State.hpp"
#include "toolbox/io/Reactor.hpp"

namespace toolbox { inline namespace io {
    class Reactor;
}}

namespace ft::core {

// TODO: pimpl wrapper ?
using Reactor = toolbox::io::Reactor;

Reactor& current_reactor();

template<typename StateT, typename ReactorT>
class BasicExecutor : public BasicComponent<BasicExecutor<StateT, ReactorT>, StateT, Identifiable> {
    using This = BasicExecutor<StateT, ReactorT>;
    using Base = BasicComponent<This, StateT, Identifiable>;
public:
    using Reactor = ReactorT;
    using State = StateT;
    using Base::state;
    using Base::state_changed;
    using Base::state_hook;
    using Base::start;
    using Base::stop;
public:
    BasicExecutor(Reactor* reactor=nullptr)
    : reactor_(reactor) {}
    Reactor& reactor() { assert(reactor_!=nullptr); return *reactor_; }
protected:
    Reactor* reactor_{nullptr};
};

using Executor = BasicExecutor<State, tb::Reactor>;

}