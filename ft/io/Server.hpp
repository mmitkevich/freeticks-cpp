#pragma once
#include "ft/core/Component.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/io/Routing.hpp"
#include "toolbox/io/Socket.hpp"
#include "toolbox/net/Endpoint.hpp"
#include "toolbox/net/Sock.hpp"
#include "toolbox/util/Slot.hpp"
#include <ft/io/Service.hpp>
#include <type_traits>
#include "ft/utils/Compat.hpp"

namespace ft::io {

template<class Self, class PeerT, class ServerSocketT, typename StateT, class ParentT, typename...ArgsT>
class BasicSocketService : public TypedParent<ParentT, BasicPeerService<Self, PeerT, StateT, ArgsT...>>
{
    FT_MIXIN(Self);
    using Base = TypedParent<ParentT, BasicPeerService<Self, PeerT, StateT, ArgsT...>>;
  public:
    using typename Base::Parent;
    using ServerSocket = ServerSocketT;
    
    static constexpr bool has_socket_accept() { return tb::SocketTraits::has_accept<ServerSocket>; }    

    using ClientSocket = typename ServerSocket::ClientSocket;
    using typename Base::Endpoint;
    using typename Base::Peer;
    using typename Base::PeersMap;
    using typename Base::Packet;

  public:
    using Base::parent, Base::peers, Base::reactor, Base::make_peer, Base::emplace_peer;
    using Base::Base;

    explicit BasicSocketService(const Endpoint& local, Parent* parent)
    :  Base(parent->reactor(), parent)
    ,  local_(local) {}

    void open() {
        socket_.open(*reactor(), local_.protocol());
        socket_.bind(local_);
    }

    /// @see SocketRef
    void close() { socket_.close(); } 

    ServerSocket& socket() { return socket_; }

    Peer& next_peer() { assert(next_peer_); return *next_peer_; }
    
    Peer* emplace_next_peer() {
        Peer* peer = next_peer_.get();
        assert(peer->parent()==self());
        emplace_peer(std::move(next_peer_));
        next_peer_ = make_next_peer();
        return peer;
    }
    
    auto make_next_peer() {
        if constexpr(has_socket_accept()) {
            return make_peer(ClientSocket(), self());
        } else {
            return make_peer(std::ref(socket()), self());
        }
    }

    void run() {
        // next possible peer
        if constexpr(has_socket_accept()) { // tcp
            socket().async_accept(next_peer().remote(), tb::bind(
            [this](ClientSocket&& socket, std::error_code ec) {
                //parent()->newpeer_(tb::ip_endpoint(remote_), ec);
                if(!ec) {
                    next_peer().socket(std::move(socket));
                    parent()->async_accept(next_peer(), tb::bind(
                    [this](std::error_code ec) {
                        if(!ec) {
                            next_peer().run([](Peer& peer, Packet& packet, tb::DoneSlot done) {
                                auto* self = static_cast<Self*>(peer.parent());
                                self->parent()->async_handle(peer, packet, done);
                            });    // peer's loop
                            run();    // accept again
                        } else {
                            on_error(ec);
                        }
                    }));
                } else {
                    on_error(ec);
                }
            }));
        } else { // udp
            next_peer().socket(std::move(std::ref(socket()))); // just ref to our socket
            struct Handler {
                void operator()(Peer& peer, Packet& packet, tb::DoneSlot done) {
                    auto* self = static_cast<Self*>(peer.parent());
                    self->async_accept_and_handle(peer, done);
                }
            };
            next_peer().template run<Handler>(); // peer's loop
        }
    }
    void async_accept_and_handle(Peer& peer, tb::DoneSlot done) {
        auto it = peers().find(peer.remote());
        if(it==peers().end()) {
            resume_ = done; // save continuation
            parent()->async_accept(peer, tb::bind(
            [p=&peer](std::error_code ec) {
                auto* self = static_cast<Self*>(p->parent());
                self->parent()->async_handle(*p, p->packet(), self->resume_.release());
            }));
        }
    }
    void on_error(std::error_code ec) {
        TOOLBOX_ERROR << "error "<<ec<<" acceptor remote="<<next_peer().remote();
    }
  protected:
    ServerSocket socket_;
    tb::DoneSlot resume_;
    Endpoint local_; /// interface to bind to
    std::unique_ptr<Peer> next_peer_ = make_next_peer();
};

template<class PeerT, class ServerSocketT, typename StateT, class ParentT>
class SocketService: public BasicSocketService<
    SocketService<PeerT, ServerSocketT, StateT, ParentT>,
    PeerT, ServerSocketT, StateT, ParentT> 
{
    using Base = BasicSocketService<
        SocketService<PeerT, ServerSocketT, StateT, ParentT>,
        PeerT, ServerSocketT, StateT, ParentT>;
  public:
    using Base::Base;
};

/// adds Acceptors
template<class Self
, class ProtocolT
, class PeerT
, class ServerSocketT
, class RoutingStrategyT
, typename StateT
>
class BasicServer : public io::BasicService<Self, typename PeerT::Reactor, StateT, core::Component>
{
    FT_MIXIN(Self);
    using Base = io::BasicService<Self, typename PeerT::Reactor, StateT, core::Component>;
  public:
    using Service = io::SocketService<PeerT, ServerSocketT, StateT, Self>;
    using Packet = typename Service::Packet;
    using Peer = PeerT;
    using ServerSocket = ServerSocketT;
    using Reactor = typename Peer::Reactor;
    using Endpoint = typename Peer::Endpoint;
    using Protocol = ProtocolT;
    using ServicesMap = tb::unordered_map<Endpoint, Service>;    
    using Routing = io::BasicRouting<Self, Peer, RoutingStrategyT>;
  public:
    using Base::Base;
    using Base::parameters;
    using Base::reactor;
    using Base::is_open;
    using Base::open, Base::close;

    //using tt = tb::SocketTraits::sock_async_accept_t<ServerSocket>;
    const ServicesMap& services() { return services_; }

    Protocol& protocol() { return protocol_; }

    void on_parameters_updated(const core::Parameters& params) {
        bool was_open = is_open();
        if(was_open)
            close();
        assert(services_.empty());
        for(auto conn_p: parameters()["connections"]) {
            auto iface = conn_p.value_or("interface", std::string{});
            auto ep = tb::TypeTraits<Endpoint>::from_string(iface);
            auto [it, is_new] = services_.emplace(ep, Service(ep, self()));
        }
        if(was_open)
            open();
    }

    void do_open() {
        for(auto& [ep, acpt]: services_) {
            acpt.open();
        }
        Base::do_open();
        protocol_.open();    // change state
        run();
    }
    
    /// accepts clients, reads messages from them and apply protocol logic
    void run() {
        for(auto& [ep, svc]: services_) {
            svc.run();
        }
    }

    /// called when service accepts peer
    void async_accept(Peer& peer, tb::DoneSlot done) {
        // FIXME: newpeer_(tb::ip_endpoint(peer.remote()));
        done({});
    }

    void do_close() {
        protocol_.close();
        for(auto& [ep, svc]: services_) {
            svc.close();
        }
        services_.clear();
    }

    void shutdown(const tb::IpEndpoint& peer) {
        Endpoint ep = Endpoint(peer.address(), peer.port());
        for(auto& [ep, svc]: services_) {
            svc.shutdown(ep);
        }
    }

    template<typename Fn>
    int for_each_peer(const Fn& fn) {
      // decide who wants this tick and broadcast
      int count = 0;
      for(auto& [ep, svc]: services_) {
        count += svc.for_each_peer(fn);
      }
      return count;
    }

    template<typename MessageT>
    void async_publish(const MessageT& message, tb::SizeSlot done) {
      for_each_peer([&message, done, this](auto& peer) {
        auto topic = message.topic();
        if(peer.subscription().test(topic)) {
            self()->async_write(peer, message, done);
        }
      });
    }

    void async_handle(Peer& peer, const Packet& packet, tb::DoneSlot done) {
        done({});
    }

    template<typename MessageT>
    void async_write(Peer& peer, const MessageT& message, tb::SizeSlot done) {
        protocol_.async_write(peer, message, done);
    }

    tb::Signal<const tb::IpEndpoint&>& newpeer() { return newpeer_; }

  protected:
    Protocol protocol_;
    ServicesMap services_; // indexed by local endpoint
    Endpoint remote_;
    tb::Signal<const tb::IpEndpoint&> newpeer_;
    Routing routing_{self()};
}; // BasicService

} // ft::io