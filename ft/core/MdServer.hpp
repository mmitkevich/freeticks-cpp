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
#include "ft/core/Service.hpp"

namespace ft { inline namespace core {

using PeerSignal = tb::Signal<const tb::IpEndpoint&>;

class IMdServer : public IService {
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


template<class Impl>
class MdServerImpl : public core::ServiceImpl<MdServerImpl<Impl>, IMdServer> {
    using Base = core::ServiceImpl<MdServerImpl<Impl>, IMdServer>;
public:

    auto* impl() { return &impl_; }
    const auto* impl() const { return &impl_; }

    template<typename...ArgsT>
    MdServerImpl(ArgsT...args)
    : impl_(std::forward<ArgsT>(args)...)
    {}

    PeerSignal& newpeer() override { return impl()->newpeer(); }
    void shutdown(const tb::IpEndpoint& peer) override { impl()->shutdown(peer); }

    void url(std::string_view url) { impl()->url(url);}
    std::string_view url() const { return impl()->url(); }

    SubscriptionSignal& subscribe(StreamTopic topic) override { return impl()->subscribe(topic); }
    core::TickSink& ticks(StreamTopic topic) override { return impl()->ticks(topic); }
    core::InstrumentSink& instruments(StreamTopic topic) override { return impl()->instruments(topic); }
private:
    Impl impl_;
};



}} // ft::core