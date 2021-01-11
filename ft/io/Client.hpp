#pragma once

#include "ft/utils/Common.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/Component.hpp"
#include "toolbox/io/Reactor.hpp"
#include "toolbox/util/RobinHood.hpp"
#include "toolbox/util/Slot.hpp"
#include <boost/mp11/algorithm.hpp>
#include <sstream>
#include <toolbox/io/Socket.hpp>
#include <ft/utils/StringUtils.hpp>
#include "ft/io/Routing.hpp"
#include "ft/core/Requests.hpp"
#include "ft/io/Service.hpp"
#include <deque>

namespace ft::io {

/// routes requests using RouterT. Derived class should implement async_write for request type
template<class Self, class RouterT, class ResponseTL>
class BasicRequestor  {
    FT_MIXIN(Self)
  public:
    template<typename T> 
    using SlotTT = tb::Slot<const T&, std::error_code>;
    using SlotTL = mp::mp_transform<SlotTT, ResponseTL>;
    using SlotTuple = mp::mp_rename<SlotTL,std::tuple>;

    template<typename RequestT, typename ResponseT>
    void async_request(const RequestT& req, tb::Slot<const ResponseT&, std::error_code> slot, tb::SizeSlot done) 
    {
        int pending = router_(self()->peers(), [&](auto& peer) { 
            self()->async_request(peer, req, slot, done ); 
        });
        if(pending==0)
            done(0, {});
        else {
            route_.pending(pending);
            route_.set_slot(done);
        }
    }

    template<typename PeerT, typename RequestT, typename ResponseT>
    void async_request(PeerT& conn, const RequestT& req, 
         SlotTT<ResponseT>  slot, tb::SizeSlot done) 
    {
        // save callback to launch after we got response
        constexpr auto indx = mp::mp_find<ResponseTL, ResponseT>::value;
        std::get< indx >(response_slots_) = slot;
        self()->async_write(conn, req, done);
    }
    void open() { router_.reset(); }
    void close() {  }
  protected:
    SlotTuple response_slots_;
    RouterT router_;
    tb::PendingSlot<ssize_t, std::error_code> route_;
}; // BasicRequestor



/// Idle timer mixin
template<typename Self>
class BasicIdleTimer
{
    FT_MIXIN(Self)
public:

    void open() {
        using namespace std::literals::chrono_literals;
        idle_timer_ = self()->reactor()->timer(tb::MonoClock::now()+1s, 10s, 
            tb::Priority::Low, tb::bind([this](tb::CyclTime now, tb::Timer& timer) {
                self()->on_idle();
        }));
    }

    void close() {
        idle_timer_.cancel();
    }

    void on_idle() { TOOLBOX_DUMP << "on_idle"; }
protected:
    tb::Timer idle_timer_;
}; // IdleTimer

template<class Self, class ProtocolT, class PeerT, typename StateT, class ParentT=core::Component>
class BasicClient: public BasicPeerService<Self, PeerT, StateT, ParentT>
{
    FT_MIXIN(Self);
    using This = BasicClient<Self, ProtocolT, PeerT, StateT, ParentT>;
    using Base = BasicPeerService<Self, PeerT, StateT, ParentT>;
  public:
    using Protocol = ProtocolT;
    using typename Base::Peer;
    using typename Base::PeersMap;
    using typename Base::Socket;
    using typename Base::Endpoint;
    using typename Base::Packet;
    using typename Base::Transport;
    using typename Base::State;
    using typename Base::Reactor;    // reactor-aware connection
  public:
    using Base::Base;

    using   Base::state, Base::parameters, 
            Base::reactor,
            Base::peers,Base::emplace,
            Base::close, Base::open, Base::is_open;

    /// application level
    Protocol& protocol() { return protocol_; }

    void on_parameters_updated(const core::Parameters& params) {
        auto was_open = is_open();
        if(was_open)
            close();

        auto connspar = parameters()["connections"];
        populate(connspar);
        protocol().on_parameters_updated(connspar);
        
        if(was_open)
            open();
    }

    void populate(const core::Parameters& params) {
        assert(peers().empty());
        for(auto p: params) {
            std::string_view type = p["type"].get_string();
            auto proto = ft::str_suffix(type, '.');
            auto iface = p.value_or("interface", std::string{});
            if(Peer::supports(proto)) {
                for(auto e : p["urls"]) {
                    std::string url {e.get_string()};
                    if(!iface.empty())
                        url = url+"|interface="+iface;
                    auto *s = self();
                    emplace(Peer(url, s));
                }
            }
        }
    }
    
    void open() {
        Base::open();
        protocol_.open();
        async_connect(tb::bind<&Self::on_connected>(self()));
    }
    
    void close() {
        protocol_.close();
        Base::close();
    }

    void on_connected(std::error_code ec) {
        TOOLBOX_INFO << "connected to "<<peers().size()<<" peers, ec "<<ec;
    }

    // connect all peers. Notifies when all done
    void async_connect(tb::DoneSlot done) {
        assert(connect_.empty());
        int pending = 0;
        for(auto& [ep, c]: Base::peers_) {
            Peer& peer = c;
            if(!peer.is_open() && !peer.is_connecting()) {
                c.async_connect(tb::bind([&peer](std::error_code ec) {
                    Self* self = static_cast<Self*>(peer.parent());
                    self->on_peer_connected(peer, ec); // after connect
                    self->connect_(ec); // calls done when all done
                }));
            }
        }
        if(pending) {
            connect_.set_slot(done);
            connect_.pending(pending);
        } else {
            done({});
        }
    }

    /// when peer connected - run peers' processing loop
    void on_peer_connected(Peer& peer, std::error_code ec) {
        if(!ec) {
            protocol().async_handshake(peer, [&peer](std::error_code ec) {
                peer.template run<Self>();
            });
        } else {
            on_error(peer, ec, "connect");
        }
    }
    
    /// specifies method to write requests (accessible from Mixins)
    template<typename RequestT>
    void async_write(Peer& peer, const RequestT& req, tb::SizeSlot done) {
        protocol().async_write(peer, req, done);
    }

    void on_error(Peer& peer, std::error_code ec, const char* what="") {
        TOOLBOX_INFO<<what<<" peer "<<peer.remote()<<" ec "<<ec;
    }
 
  protected:
    Protocol protocol_;
    tb::PendingSlot<std::error_code>  connect_;
}; // BasicClient

} // namespace ft::io