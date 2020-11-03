#pragma once

#include "ft/core/Identifiable.hpp"
#include "ft/utils/Common.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/Executor.hpp"
#include "toolbox/io/State.hpp"
#include <deque>
#include <atomic>
#include <exception>

namespace ft::core {

using State = toolbox::io::State;
using ExceptionPtr = const std::exception*;

template<typename StateT>
using StateSignal = tb::Signal<StateT, StateT, ExceptionPtr>;

class IComponent : public IParameterized {
public:
    using StateSignal = core::StateSignal<State>;

    virtual ~IComponent() = default;
    /// 
    virtual void start() = 0;
    virtual void stop() = 0;
    
    // set url
    virtual void url(std::string_view url) = 0;
    virtual std::string_view url() const = 0;

    virtual State state() const = 0;    
    virtual StateSignal& state_changed() = 0;
};

template<typename ImplT>
class Component : public IComponent {
public:
    Component(std::unique_ptr<ImplT> &&impl)
    : impl_(std::move(impl)) {}

    void start() override { impl_->start(); }
    void stop() override { impl_->stop(); }
    
    void url(std::string_view url) { impl_->url(url);}
    std::string_view url() const { return impl_->url(); }

    void parameters(const Parameters& parameters, bool replace=false) override { impl_->parameters(parameters, replace); }
    const Parameters& parameters() const override { return impl_->parameters(); }
    ParametersSignal& parameters_updated() override { return impl_->parameters_updated(); }

protected:
    std::unique_ptr<ImplT> impl_;
};

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

template<typename DerivedT, typename StateT=State, typename BaseT=Identifiable>
class BasicComponent : public BaseT {
public:
    using Base = BaseT;
    using Base::Base;
    using State = StateT;
    using StateSignal = tb::Signal<State, State, ExceptionPtr>;

    void start() {
        state(State::Starting);
        state(State::Started);
    }

    void stop() {
        state(State::Stopping);
        state(State::Stopped);
    }
    
    void state(State state, ExceptionPtr err=nullptr) {
        if(state != state_) {
            auto old_state = state_.load(std::memory_order_acquire);
            state_.store(state, std::memory_order_release);
            switch(state) {
                case State::Started: on_started_.invoke(); break;
                case State::Stopped: on_stopped_.invoke(); break;
            }
            state_changed_.invoke(state, old_state, err);
        }
    }
    State state() const { return state_; }

    template<typename FnT>
    bool state_hook(State state, FnT&& fn) {
        if(state_==state) {
            fn();
            return true;
        }
        switch(state) {
            case State::Started: on_started_.emplace_back(std::move(fn)); return false;
            case State::Stopped: on_stopped_.emplace_back(std::move(fn)); return false;
            default: assert(false); return false;
        }
    }
    StateSignal& state_changed() { return state_changed_; }
protected:
    std::atomic<State> state_{State::Stopped};
    StateSignal state_changed_; 
    Completions on_stopped_;
    Completions on_started_;
};

} // ns ft::core