#pragma once

#include "ft/core/Component.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/State.hpp"
#include "toolbox/net/Endpoint.hpp"

namespace ft { inline namespace core {

using NewPeerSignal = tb::Signal<PeerId, const tb::IpEndpoint&>;

class IService : public IComponent, public IStateful, public IParameterized {
public:
    virtual void start() = 0;
    virtual void stop() = 0;
    
    virtual State state() const = 0;    
    virtual StateSignal& state_changed() = 0;
};

template<class Self, class Base=IService>
class ServiceImpl
: public ParameterizedImpl<Self, Base>
, public StatesImpl<Self, Base>
, public ComponentImpl<Self, Base>
{
    // forward decision on impl to derived
    auto* impl() { return static_cast<Self*>(this)->impl(); }
    const auto* impl() const { return static_cast<const Self*>(this)->impl(); }
public:
    void start() override { impl()->start(); };
    void stop() override { impl()->stop(); }
};

}} // ft::core