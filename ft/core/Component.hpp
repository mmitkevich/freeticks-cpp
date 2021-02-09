#pragma once
#include "ft/core/Ref.hpp"
#include "ft/core/Identifiable.hpp"
#include "ft/utils/Common.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/State.hpp"
#include <boost/type_traits/is_detected.hpp>
#include <atomic>


namespace ft { inline namespace core {

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

/// Final class of CRTP hierarchy
template<class ImplT, template<class SelfT, typename...> class BaseTT> 
class Proxy : public BaseTT<Proxy<ImplT, BaseTT>>  {
public:
    /// forward ctor
    template<typename...ArgsT>
    Proxy(ArgsT...args)
    : impl_(std::forward<ArgsT>(args)...) {}
    
    /// selects actual impl for base
    auto* impl() { return &impl_; }
    const auto* impl() const { return &impl_; }
protected:
    ImplT impl_;
};

class Component;

class IComponent {
public:
    FT_IFACE(IComponent);

    virtual ~IComponent() = default;
    virtual Component* parent() = 0;
};

/// implements stuff for IComponent taking actual implementation from derived class
template<class SelfT, class BaseT=core::IComponent>
class ComponentImpl : virtual public BaseT {
public:    
    FT_IMPL(SelfT);
    Component* parent() override { return impl()->parent(); }
protected:
    std::unique_ptr<IComponent> parent_{};
};

/// Component = HasParent<Component> + Identifiable + Moveable 
class Component :  public Identifiable, public Movable {
public:
    using Parent = Component;
    using Base = Identifiable;
    using Ref = BasicRef<IComponent>;
    
    explicit Component(Component* parent=nullptr, Identifier id={})
    : Identifiable(parent, id)
    , parent_(parent)
    {}

    Component* parent() { return parent_;}
    const Component* parent() const { return parent_; }
    void parent(Component* val) { parent_ = val;}
protected:
    Component* parent_{};
};

}} // ns ft::core