#pragma once

#include "ft/core/Identifiable.hpp"
#include "ft/utils/Common.hpp"
#include "ft/core/Parameters.hpp"
#include "toolbox/io/State.hpp"
#include "toolbox/io/Reactor.hpp"
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
class ComponentImpl : public IComponent {
public:
    ComponentImpl(std::unique_ptr<ImplT> &&impl)
    : impl_(std::move(impl)) {}

    void start() override { impl_->start(); }
    void stop() override { impl_->stop(); }
    
    void url(std::string_view url) { impl_->url(url);}
    std::string_view url() const { return impl_->url(); }

    void parameters(const Parameters& parameters, bool replace=false) override { impl_->parameters(parameters, replace); }
    const Parameters& parameters() const override { return impl_->parameters(); }
protected:
    std::unique_ptr<ImplT> impl_;
};

/// list of deferred hooks
template<typename...ArgsT>
class BasicHooks {
public:
    template<typename F>
    void connect(F&& fn) {
        fns_.emplace_back(std::move(fn));
    }
    void invoke(ArgsT...args) {
        while(!fns_.empty()) {
            auto hook = std::move(fns_.front());
            fns_.pop_front();
            hook(std::forward<ArgsT>(args)...);
        }
    }
    void operator()(ArgsT...args) { invoke(std::forward<ArgsT>(args)...); }
private:
    std::deque<std::function<void(ArgsT...)>> fns_;
};


using Reactor = toolbox::io::Reactor;

Reactor& current_reactor();

class Component : public Identifiable {
public:
    Reactor& reactor() { return *reactor_; }
    Component(Reactor*reactor=nullptr)
    : reactor_(reactor)
    {}
    void url(std::string_view url) { url_ = url; }
    std::string_view url() const { return url_; }
protected:
    Reactor *reactor_{nullptr};
    std::string url_;
};
/// Component: identifier + parameters + state + reactor
template<typename DerivedT, typename StateT>
class BasicComponent : public Component, public BasicParameterized<DerivedT> {
public:
    using Base = Component;
    using State = StateT;
    using StateSignal = tb::Signal<State, State, ExceptionPtr>;
public:
    using Base::Base;
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
                case State::Started: started_hooks_.invoke(); break;
                case State::Stopped: stopped_hooks_.invoke(); break;
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
            case State::Started: started_hooks_.connect(std::move(fn)); return false;
            case State::Stopped: stopped_hooks_.connect(std::move(fn)); return false;
            default: assert(false); return false;
        }
    }
    StateSignal& state_changed() { return state_changed_; }
protected:
    std::atomic<State> state_{State::Stopped};
    StateSignal state_changed_; 
    BasicHooks<> stopped_hooks_;
    BasicHooks<> started_hooks_;
    Reactor* reactor_;
};



} // ns ft::core