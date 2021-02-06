#pragma once

#include "ft/core/Component.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/State.hpp"
#include "ft/core/Stream.hpp"
#include "toolbox/net/Endpoint.hpp"

namespace ft { inline namespace core {

using NewPeerSignal = tb::Signal<PeerId, const tb::IpEndpoint&>;

class IService : public IComponent, public IStateful, public IParameterized {
public:
    FT_IFACE(IService);

    /// start service
    virtual void start() = 0;
    /// stop service
    virtual void stop() = 0;
    /// get (input) stream associated with topic
    virtual Stream& signal(StreamTopic topic) = 0;
    /// get (output) stream associated with topic
    virtual Stream& slot(StreamTopic topic) = 0;
    /// very basic stats
    //virtual core::StreamStats& stats() = 0;
    virtual State state() const = 0;    
    virtual StateSignal& state_changed() = 0;
};


template<class Self, class Base>
class IService::Impl
: public ParameterizedImpl<Self, Base>
, public StatesImpl<Self, Base>
, public ComponentImpl<Self, Base>
{
    FT_IMPL(Self)
public:
    void start() override { impl()->start(); };
    void stop() override { impl()->stop(); }
    Stream& signal(StreamTopic topic) override { return impl()->signal(topic); }
    Stream& slot(StreamTopic topic) override { return impl()->slot(topic); }
};


}} // ft::core