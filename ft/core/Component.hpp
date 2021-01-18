#pragma once

#include "ft/core/Identifiable.hpp"
#include "ft/utils/Common.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/State.hpp"
#include <boost/type_traits/is_detected.hpp>
#include <atomic>


namespace ft { inline namespace core {


/*
/// convert some template into single-argument mixin template
template<template<typename, auto...> class TemplateT, auto...ArgsT>
struct Bind {
    template<class Self>
    using Type = TemplateT<Self, ArgsT...>;
};
*/

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

class Component;

class IComponent {
public:
    virtual ~IComponent() = default;
    virtual Component* parent() = 0;
};

/// implements stuff for IComponent taking actual implementation from derived class
template<class Self, class Base=core::IComponent>
class ComponentImpl : virtual public Base {
public:    
    // forward decision on impl to derived
    auto* impl() { return static_cast<Self*>(this)->impl(); }
    const auto* impl() const { return static_cast<const Self*>(this)->impl(); }

    Component* parent() override { return impl()->parent(); }
protected:
    std::unique_ptr<IComponent> parent_{};
};

/// Component = HasParent<Component> + Identifiable + Moveable 
class Component :  public Identifiable, public Movable {
public:
    using Parent = Component;
    
    explicit Component(Component* parent=nullptr)
    : parent_(parent)
    {}
    Component* parent() { return parent_;}
    const Component* parent() const { return parent_; }
    void parent(Component* val) { parent_ = val;}
protected:
    Component* parent_{};
};

/// declares Parent type
template<class ParentT=core::Component, class Base=core::Component>
class TypedParent : public Base {
  public:
    using Parent = ParentT;
    
    using Base::Base;

    Parent* parent() { return static_cast<Parent*>(Base::parent()); }
    const Parent* parent() const { return static_cast<const Parent*>(Base::parent());  }
    void parent(Parent* val) { Base::parent(val); }
};

}} // ns ft::core