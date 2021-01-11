#pragma once

#include "ft/core/Instrument.hpp"
#include "ft/core/Requests.hpp"
#include "ft/core/Stream.hpp"
#include "ft/core/StreamStats.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/Tick.hpp"
#include "ft/utils/Common.hpp"
#include "ft/core/Component.hpp"
#include "toolbox/net/Endpoint.hpp"
#include "toolbox/util/Slot.hpp"


namespace ft { inline namespace core {

using PeerSignal = tb::Signal<const tb::IpEndpoint&>;

class IMdServer : public IComponent {
public:
    virtual SubscriptionSignal& subscribe(StreamTopic topic) = 0;

    /// Connection of new peer
    virtual PeerSignal& newpeer() = 0;

    /// force close peer
    virtual void shutdown(const tb::IpEndpoint& ep) = 0;

    /// topic="instruments"
    virtual core::InstrumentSink& instruments(StreamTopic topic) = 0;
    /// topic = "bestprice", "orderbook", ...
    virtual core::TickSink& ticks(StreamTopic topic) = 0;
};


template<typename ImplT>
class MdServerImpl : public IMdServer {
public:
    MdServerImpl(std::unique_ptr<ImplT> &&impl)
    : impl_(std::move(impl)) {}

    void start() override { impl_->start(); }
    void stop() override { impl_->stop(); }
    PeerSignal& newpeer() override { return impl_->newpeer(); }
    void shutdown(const tb::IpEndpoint& peer) override { impl_->shutdown(peer); }

    void url(std::string_view url) { impl_->url(url);}
    std::string_view url() const { return impl_->url(); }

    State state() const override { return impl_->state(); }
    StateSignal& state_changed() override { return impl_->state_changed(); };

    void parameters(const Parameters& parameters, bool replace=false) override { impl_->parameters(parameters, replace); }
    const Parameters& parameters() const override { return impl_->parameters(); }
    
    SubscriptionSignal& subscribe(StreamTopic topic) override { return impl_->subscribe(topic); }
    core::TickSink& ticks(StreamTopic topic) override { return impl_->ticks(topic); }
    core::InstrumentSink& instruments(StreamTopic topic) override { return impl_->instruments(topic); }
private:
    std::unique_ptr<ImplT> impl_;
};



}} // ft::core