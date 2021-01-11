#pragma once
#include "ft/core/Requests.hpp"
#include "ft/utils/Common.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/Component.hpp"
#include "ft/core/MdClient.hpp"
#include "toolbox/net/EndpointFilter.hpp"

#include "toolbox/sys/Time.hpp"
#include "toolbox/util/Slot.hpp"
#include "toolbox/util/String.hpp"

#include "ft/io/Client.hpp"
#include "ft/io/Conn.hpp"

#include "ft/core/Tick.hpp"
#include "ft/core/Instrument.hpp"

#include <string_view>
#include <system_error>
#include <vector>

namespace ft::io {

template< class Self
, class ProtocolT
, class PeerT
, typename StateT
, class RouterT = RoundRobinRoutingStrategy<PeerT>
, class RequestorT = BasicRequestor<Self, RouterT, mp::mp_list<core::SubscriptionResponse>>
, class IdleTimerT = BasicIdleTimer<Self>
> class BasicMdClient : public io::BasicClient<Self, ProtocolT, PeerT, StateT>, RequestorT, IdleTimerT
{
    using Base = BasicClient<Self, ProtocolT, PeerT, StateT>;
    using Requestor = RequestorT;
    using IdleTimer = IdleTimerT;
    friend Base;
    friend Requestor;
    friend IdleTimer;
public:
    using typename Base::Protocol;
    using typename Base::Peer;
    using typename Base::PeersMap;
    using typename Base::Packet;
    using typename Base::Socket;
    using typename Base::Endpoint;
    using typename Base::State;
public:
    using Base::Base;
    using Base::state, Base::reactor, Base::protocol;
    using Base::peers;
    using Base::async_connect;
    using Requestor::async_request;

    auto& stats() { return protocol().stats(); }

    void open() {
        Base::open();
        Requestor::open();
        IdleTimer::open();
    }

    void close() {
        IdleTimer::close();
        Requestor::close();
        Base::close();
    }

    /// forward to protocol
    void async_handle(Peer& peer, const Packet& packet, tb::DoneSlot done) {
        protocol().async_handle(peer, packet, done);     // will do all the stuff, then call done()
    }


    /// connectable streams
    core::TickStream& ticks(core::StreamTopic topic) { return protocol().ticks(topic); }

    core::InstrumentStream& instruments(core::StreamTopic topic) { return protocol().instruments(topic); }

protected:

    void on_idle() {
        std::stringstream ss;
        protocol().stats().report(ss);
        TOOLBOX_INFO << ss.str();
    }
}; // BasicMdClient

template<class ProtocolT, class PeerT, typename StateT=core::State>
class MdClient : public BasicMdClient<MdClient<ProtocolT, PeerT, StateT>, ProtocolT, PeerT, StateT> {
    using Base = BasicMdClient<MdClient<ProtocolT, PeerT, StateT>, ProtocolT, PeerT, StateT>;
  public:
    using Base::Base;
};

} // ft::io