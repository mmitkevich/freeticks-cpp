#pragma once
#include "ft/utils/Common.hpp"
#include "ft/core/Instrument.hpp"
#include "ft/core/InstrumentsCache.hpp"
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

// interface is not dependent on concrete stream types...
class IServer : public IService {
public:
    FT_IFACE(IServer);

    /// peer subscribed
    virtual SubscriptionSignal& subscription() = 0;

    /// Connection of new peer
    virtual NewPeerSignal& newpeer() = 0;

    /// force close peer
    virtual void shutdown(PeerId peer) = 0;

    virtual void instruments_cache(core::InstrumentsCache* cache) = 0;

    /// typed slot
    template<typename...ArgsT>
    Stream::Slot<ArgsT...>& slot_of(StreamTopic topic) {
        return Stream::Slot<ArgsT...>::from(this->slot(topic));
    }

};


template<class Self, class Base>
class IServer::Impl : public core::IService::Impl<IServer::Impl<Self, Base>, Base> {
public:
    FT_IMPL(Self);

    NewPeerSignal& newpeer() override { return impl()->newpeer(); }
    void shutdown(PeerId peer) override { impl()->shutdown(peer); }

    void instruments_cache(core::InstrumentsCache* cache) override { impl()->instruments_cache(cache); }

    void url(std::string_view url) { impl()->url(url);}
    std::string_view url() const { return impl()->url(); }

    SubscriptionSignal& subscription() override { return impl()->subscription(); }
};

}} // ft::core