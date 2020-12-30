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
#include "ft/io/Router.hpp"
#include "ft/io/Service.hpp"
#include <deque>

namespace ft::io {

template<typename PeersMapT>
class RouteRoundRobin {
public:
    using PeersMap = PeersMapT;
    using Peer = typename PeersMap::mapped_type;
    using iterator = typename PeersMap::iterator;

    template<typename RequestT>
    Peer* operator()(PeersMap& peers, RequestT& req)  {
        if(peers.empty())
            return nullptr;
        if(it_!=iterator() && it_!=peers.end())
            ++it_;
        else 
            it_ = peers.begin();
        return &it_->second;
    }
    void reset() { it_ = iterator(); }
protected:
    iterator it_{};
};

/// Mixin for request-response processing
template<typename DerivedT, typename PeerT, typename ResponseTL>
class BasicRequestResponseClient {
public:
    using Peer = PeerT;
    using Self = DerivedT;
    template<typename T> 
    using SlotTT = tb::Slot<const T&, std::error_code>;
    using SlotTL = mp::mp_transform<SlotTT, ResponseTL>;
    using SlotTuple = mp::mp_rename<SlotTL,std::tuple>;

    Self* self() { return static_cast<Self*>(this); }
    const Self* self() const { return static_cast<const Self*>(this); }

    template<typename RequestT, typename ResponseT>
    void async_request(const RequestT& req, tb::Slot<const ResponseT&, std::error_code> slot, tb::SizeSlot done) 
    {
        auto* conn = self()->route_request(req);
        if(conn)
            self()->async_request(*conn, req, slot, done);
    }

    /// nothere
    template<typename RequestT>
    Peer* route_request(const RequestT& req) { 
        assert(false); // TODO: not_implemented_error
        return nullptr;
    } 
    
    template<typename RequestT, typename ResponseT>
    void async_request(Peer& conn, const RequestT& req, 
         SlotTT<ResponseT>  slot, tb::SizeSlot done) 
    {
        // save callback to launch after we got response
        constexpr auto indx = mp::mp_find<ResponseTL, ResponseT>::value;
        std::get< indx >(response_slots_) = slot;
        self()->async_write(conn, req, done);
    }
    
protected:
    SlotTuple response_slots_;
};

/// Client = PeerService + Connectable
template<typename DerivedT, 
    typename ProtocolT,
    typename PeerT, // BasicConn<>
    typename ResponseTL // typelist with possible response types
> class BasicClient : public io::BasicPeersService<DerivedT, ProtocolT, PeerT, core::State>
, public io::BasicRequestResponseClient<DerivedT, PeerT, ResponseTL>
{
    using This = BasicClient<DerivedT, ProtocolT, PeerT, ResponseTL>;
    using Base = io::BasicPeersService<DerivedT, ProtocolT, PeerT, core::State>;

    using Self = DerivedT;
    Self* self() { return static_cast<Self*>(this);}
    const Self* self() const { return static_cast<const Self*>(this);}
public:
    using typename Base::Protocol;
    using typename Base::Peer;
    using typename Base::PeersMap;
    using typename Base::Socket;
    using typename Base::Endpoint;
    using typename Base::Packet;
    using typename Base::Transport;
    using typename Base::State;
    using typename Base::Reactor;    // reactor-aware connection
    using RequestResponse = io::BasicRequestResponseClient<DerivedT, PeerT, ResponseTL>;  
public:
    using Base::Base;

    using   Base::state, Base::parameters, 
            Base::reactor, Base::transport, Base::protocol,
            Base::peers, Base::close, Base::emplace;

    void on_parameters_updated(const core::Parameters& params) {
        for(auto p: parameters()["connections"]) {
            std::string_view type = p["type"].get_string();
            auto proto = ft::str_suffix(type, '.');
            auto iface = p.value_or("interface", std::string{});
            if(Peer::supports(proto)) {
                for(auto e : p["urls"]) {
                    std::string url {e.get_string()};
                    if(!iface.empty())
                        url = url+"|interface="+iface;
                    emplace(Peer(url));
                }
            }
        }
    }

    // connect all connections not connected yet 
    void async_connect(tb::DoneSlot done) {
        auto pending = std::count_if(peers().begin(), peers().end(), [](auto& it) {
            return !it.second.is_open();
        });
        if(pending==0) {
            done({});
        }
        assert(connect_.empty());
        connect_.set_slot(done);
        connect_.pending() = pending;
        for(auto& [ep, c]: peers()) {
            if(!c.is_open() && !c.is_connecting()) {
                c.async_connect(tb::bind([peer=&c](std::error_code ec) {
                    Self* self = static_cast<Self*>(peer->service());
                    self->connect_.pending()--;
                    self->async_handshake(*peer, tb::bind([self](std::error_code ec) {
                        self->connect_(ec);
                    }));
                }));
            }
        }   
    }

    void async_handshake(Peer& c, tb::DoneSlot slot) {
        protocol().async_handshake(c, slot);
    }

    void on_open() {
        router_.reset();
        using namespace std::literals::chrono_literals;
        idle_timer_ = reactor()->timer(tb::MonoClock::now()+1s, 10s, 
            tb::Priority::Low, tb::bind([this](tb::CyclTime now, tb::Timer& timer) {
                self()->on_idle();
            }));
    }

    void on_close() {
        router_.reset();
        idle_timer_.cancel();
    }

    ///  background reporting
    void on_idle() {}

    /// from Router
    using Base::async_write;
    
    /// from RequestResponseClient
    using RequestResponse::async_request;
 
    /// process using protocol
    void async_process(Peer& src, Packet& packet, tb::Slot<std::error_code> done) {
        protocol().async_process(src, packet, done);
    }

    template<typename RequestT>
    Peer* route_request(const RequestT& req) {  return router_(peers(), req); }
protected:
    tb::PendingSlot<std::error_code>  connect_;
    tb::Timer idle_timer_; 
    io::RouteRoundRobin<PeersMap> router_;
};

} // namespace ft::io