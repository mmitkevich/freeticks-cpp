#pragma once
#include "ft/core/Requests.hpp"
//#include "ft/io/Routing.hpp"
#include "ft/utils/Common.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/Component.hpp"
#include "ft/core/Client.hpp"
#include "toolbox/io/Socket.hpp"
#include "toolbox/net/EndpointFilter.hpp"

#include "toolbox/sys/Time.hpp"
#include "toolbox/util/Slot.hpp"
#include "toolbox/util/String.hpp"

#include "ft/io/Client.hpp"
#include "ft/io/Conn.hpp"

#include "ft/core/Tick.hpp"
#include "ft/core/Instrument.hpp"

#include <boost/mp11/detail/mp_list.hpp>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <vector>

namespace ft::io {

template< class Self
, class ProtocolT
, class PeerT
, typename StateT
//, class RouterT = BasicRouter<Self, PeersMap<PeerT>, RoutingT>
, class IdleTimerT = BasicIdleTimer<Self>
> class BasicMdClient : public io::BasicClient<Self, ProtocolT, PeerT, StateT>
, public IdleTimerT
{
    FT_SELF(Self);
    using Base = BasicClient<Self, ProtocolT, PeerT, StateT>;
  //  using Router = RouterT;
    using IdleTimer = IdleTimerT;
    friend Base;
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
    using Base::peers, Base::for_each_peer;
    using Base::async_connect;
    using Base::async_write;
    
    auto& stats() { return protocol().stats(); }

    void open() {
        Base::open();
        IdleTimer::open();
    }

    void close() {
        IdleTimer::close();
        Base::close();
    }

    /// forward to protocol
    void async_handle(Peer& peer, const Packet& packet, tb::DoneSlot done) {
        protocol().async_handle(peer, packet, done);     // will do all the stuff, then call done()
    }
    
    template<typename MessageT>
    void async_write(const MessageT& m, tb::SizeSlot done) {
        write_.set_slot(done);
        write_.pending(0);
        for_each_peer([&](auto& peer) {
            if(self()->check(peer, m)) {
              write_.inc_pending();
              self()->async_write(peer, m, write_.get_slot());
            }
        });
    }
    template<typename MessageT>
    bool check(Peer& peer, const MessageT& m) {
        /*StreamTopic topic = m.topic();
        auto& strm = stream(topic);
        bool result = strm.subscription().check(m);
        return result;*/
        return true;    // send "subscribe" to every peer for now. FIXME: order routing logic ?
    }

    /// connectable streams
    core::Stream& stream(core::StreamTopic topic) { 
        return protocol().stream(topic);
        /*switch(topic) {
            case core::StreamTopic::BestPrice: return protocol().ticks(topic); 
            case core::StreamTopic::Instrument: return protocol().instruments(topic);
            default: throw std::logic_error(std::string{"stream not supported"});
        }*/
    }
protected:
    void on_idle() {
        std::stringstream ss;
        protocol().stats().report(ss);
        TOOLBOX_INFO << ss.str();
    }
protected:
    //Router router_{self()};
    tb::PendingSlot<ssize_t, std::error_code> write_;
}; // BasicMdClient

template<class ProtocolT, class PeerT, typename StateT=core::State>
class MdClient : public BasicMdClient<MdClient<ProtocolT, PeerT, StateT>, ProtocolT, PeerT, StateT>
{
    using Base = BasicMdClient<MdClient<ProtocolT, PeerT, StateT>, ProtocolT, PeerT, StateT>;
  public:
    using Base::Base;
};

} // ft::io