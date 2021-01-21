#pragma once
#include "ft/core/Requests.hpp"
#include "ft/io/Routing.hpp"
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

#include <boost/mp11/detail/mp_list.hpp>
#include <string_view>
#include <system_error>
#include <vector>

namespace ft::io {

template< class Self
, class ProtocolT
, class PeerT
, class RoutingT
, typename StateT
, class RouterT = BasicRouter<Self, PeerT, RoutingT>
, class IdleTimerT = BasicIdleTimer<Self>
> class BasicMdClient : public io::BasicClient<Self, ProtocolT, PeerT, StateT>
, public RouterT
, public IdleTimerT
{
    using Base = BasicClient<Self, ProtocolT, PeerT, StateT>;
    using Router = RouterT;
    using IdleTimer = IdleTimerT;
    friend Base;
    friend Router;
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
    using Base::async_write;
    using Router::async_write;
    
    auto& stats() { return protocol().stats(); }

    void open() {
        Base::open();
        Router::open();
        IdleTimer::open();
    }

    void close() {
        IdleTimer::close();
        Router::close();
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

template<class ProtocolT, class PeerT, class RoutingT=RoundRobinRouting<PeerT>, typename StateT=core::State>
class MdClient : public BasicMdClient<MdClient<ProtocolT, PeerT, RoutingT, StateT>, ProtocolT, PeerT, RoutingT, StateT>
{
    using Base = BasicMdClient<MdClient<ProtocolT, PeerT, RoutingT, StateT>, ProtocolT, PeerT, RoutingT, StateT>;
  public:
    using Base::Base;
};

} // ft::io