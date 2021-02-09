#pragma once
#include "ft/utils/Common.hpp"
#include "ft/core/Ref.hpp"
#include "toolbox/io/State.hpp"
#include <exception>
#include <deque>

namespace ft { inline namespace core {


using State = toolbox::io::State;
using ExceptionPtr = const std::exception*;

template<typename StateT>
using BasicStateSignal = tb::Signal<StateT, StateT, ExceptionPtr>;

class IStateful {
public:
    using Ref = BasicRef<IStateful>;

    using State = core::State;
    using StateSignal = core::BasicStateSignal<State>;
    virtual ~IStateful() = default;
    virtual State state() const = 0;
    virtual StateSignal& state_changed() = 0;
};

template<class Self, class Base=core::IStateful>
class StatesImpl : virtual public Base {
    auto* impl() { return static_cast<Self*>(this)->impl(); }
    const auto* impl() const { return static_cast<const Self*>(this)->impl(); }
public:
    using typename Base::State;
    using typename Base::StateSignal;

    State state() const override { return impl()->state(); }
    StateSignal& state_changed() override { return impl()->state_changed(); };
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


/// State with hooks
template<typename StateT>
class BasicStateful {
  public:
    using State = StateT;
    using StateSignal = BasicStateSignal<State>;
  public:
    void state(State state, ExceptionPtr err=nullptr) {
        if(state != state_) {
            auto old_state = state_;
            state_ = state;
            switch(state) {
                case State::Open: started_hooks_.invoke(); break;
                case State::Closed: stopped_hooks_.invoke(); break;
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
            case State::Open: started_hooks_.connect(std::move(fn)); return false;
            case State::Closed: stopped_hooks_.connect(std::move(fn)); return false;
            default: assert(false); return false;
        }
    }
    StateSignal& state_changed() { return state_changed_; }
protected:
    State state_ {};
    StateSignal state_changed_; 
    BasicHooks<> stopped_hooks_;
    BasicHooks<> started_hooks_;
};


}} // ft::core