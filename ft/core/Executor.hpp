#pragma once
#include "ft/utils/Common.hpp"
#include <deque>
#include <atomic>
#include <exception>

#include "toolbox/io/State.hpp"
#include "toolbox/io/Reactor.hpp"

namespace toolbox { inline namespace io {
    class Reactor;
}}

namespace ft::core {

// TODO: pimpl wrapper ?
using Reactor = toolbox::io::Reactor;
using RunState = toolbox::io::State;

Reactor& current_reactor();

using ExceptionPtr = const std::exception*;
using RunStateSignal = tb::Signal<RunState, RunState, ExceptionPtr>;



class Completions {
public:
    template<typename F>
    void emplace_back(F&& fn) {
        fns_.emplace_back(std::move(fn));
    }
    void invoke() {
        while(!fns_.empty()) {
            fns_.front()();
            fns_.pop_front();
        }
    }
private:
    std::deque<std::function<void()>> fns_;
};

class Executor {
public:
    Executor(tb::Reactor* reactor=nullptr)
    : reactor_(reactor) {}
    tb::Reactor& reactor() { assert(reactor_!=nullptr); return *reactor_; }
    RunStateSignal& state_changed() { return state_changed_; }
    void state(RunState state, ExceptionPtr err=nullptr) {
        if(state != state_) {
            auto old_state = state_.load(std::memory_order_acquire);
            state_.store(state, std::memory_order_release);
            switch(state) {
                case RunState::Started: on_started_.invoke(); break;
                case RunState::Stopped: on_stopped_.invoke(); break;
            }
            state_changed_.invoke(state, old_state, err);
        }
    }
    RunState state() const { return state_; }
    template<typename FnT>
    bool on_state(RunState state, FnT&& fn) {
        if(state_==state) {
            fn();
            return true;
        }
        switch(state) {
            case RunState::Started: on_started_.emplace_back(std::move(fn)); return false;
            case RunState::Stopped: on_stopped_.emplace_back(std::move(fn)); return false;
            default: assert(false); return false;
        }
    }
protected:
    tb::Reactor* reactor_{nullptr};
    std::atomic<RunState> state_{RunState::Stopped};
    RunStateSignal state_changed_; 
    Completions on_stopped_;
    Completions on_started_;
};

}