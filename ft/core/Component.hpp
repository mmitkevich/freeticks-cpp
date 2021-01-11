#pragma once

#include "ft/core/Identifiable.hpp"
#include "ft/utils/Common.hpp"
#include "ft/core/Parameters.hpp"
#include "toolbox/io/State.hpp"
#include "toolbox/io/Reactor.hpp"
#include <boost/type_traits/is_detected.hpp>
#include <deque>
#include <atomic>
#include <exception>

namespace ft { inline namespace core {

/// CRTP boilerplate
#define FT_MIXIN(Self) \
    constexpr Self* self() { return static_cast<Self*>(this); } \
    constexpr const Self* self() const { return static_cast<const Self*>(this); } 

/// convert some template into single-argument mixin template
template<template<typename, auto...> class TemplateT, auto...ArgsT>
struct Bind {
    template<class Self>
    using Type = TemplateT<Self, ArgsT...>;
};


class Movable {
public:
    Movable(const Movable&) = delete;
    Movable&operator=(const Movable&) = delete;
    Movable(Movable&&) = default;
    Movable&operator=(Movable&&) = default;
    Movable() = default;
};

class NonMovable {
public:
    NonMovable(const NonMovable&) = delete;
    NonMovable&operator=(const NonMovable&) = delete;
    NonMovable(NonMovable&&) = default;
    NonMovable&operator=(NonMovable&&) = default;
    NonMovable() = default;
};

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

    virtual State state() const = 0;    
    virtual StateSignal& state_changed() = 0;
};

template<class Self>
class ComponentImpl : public IComponent {
    Self* self() { return self_; }
public:
    ComponentImpl(std::unique_ptr<Self> &&self)
    : self_(std::move(self)) {}

    void start() override { self()->start(); }
    void stop() override { self()->stop(); }

    void parameters(const Parameters& parameters, bool replace=false) override { 
        self()->parameters(parameters, replace);
    }
    const Parameters& parameters() const override { return self()->parameters(); }
protected:
    std::unique_ptr<Self> self_;
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

/// Component = HasParent<Component> + Identifiable + Moveable 
class Component :  public Identifiable, public Movable {
    using Base = Identifiable;
public:
    Component(const Component&) = delete;
    Component&operator=(const Component&) = delete;
    Component(Component&&) = default;
    Component& operator=(Component&&) = default;
    Component() = default;
    
    explicit Component(Component* parent)
    : parent_(parent)
    {}

    Component* parent() { return parent_;}
    const Component* parent() const { return parent_; }
    void parent(Component* val) { parent_ = val;}
protected:
protected:
    Component* parent_{};
};


/// State with hooks
template<typename StateT>
class BasicStates {
  public:
    using State = StateT;
    using StateSignal = tb::Signal<State, State, ExceptionPtr>;
  public:
    void state(State state, ExceptionPtr err=nullptr) {
        if(state != state_) {
            auto old_state = state_;
            state_ = state;
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
    State state_ {};
    StateSignal state_changed_; 
    BasicHooks<> stopped_hooks_;
    BasicHooks<> started_hooks_;
};

/// Component<Parent> = Component[Identifiable+Movable+Child<Component>] + Child<Parent> + Url
template<class Self, class ParentT=core::Component, class BaseT=core::Component>
class BasicComponent : public BaseT {
  FT_MIXIN(Self);
  private:
    template<typename T>
    using parent_t = decltype(std::declval<T&>().parent());
    static_assert(boost::is_detected_v<parent_t, ParentT>, "ParentT should declare parent()");
    static_assert(boost::is_detected_v<parent_t, BaseT>, "BaseT should declare parent()");
  public:
    using BaseT::BaseT;
    using Parent = ParentT; 
    
    Parent* parent() { return static_cast<Parent*>(BaseT::parent()); }
    const Parent* parent() const { return static_cast<const Parent*>(BaseT::parent());  }
    void parent(Parent* val) { BaseT::parent(val); }
};

}} // ns ft::core