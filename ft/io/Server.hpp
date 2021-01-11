#pragma once
#include "ft/core/Component.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/io/Routing.hpp"
#include "toolbox/io/Socket.hpp"
#include "toolbox/net/Endpoint.hpp"
#include "toolbox/util/Slot.hpp"
#include <ft/io/Service.hpp>

namespace ft::io {

template<class Self, class PeerT, class SocketT, typename StateT, class ParentT>
class BasicSocketService : public BasicPeerService<Self, PeerT, StateT, ParentT>
{
    FT_MIXIN(Self);
    using Base = BasicPeerService<Self, PeerT, StateT, ParentT>;
  public:
    using typename Base::Parent;
    using Socket = SocketT;
    using ClientSocket = typename Socket::ClientSocket;
    using typename Base::Endpoint;
    using typename Base::Peer;
    using typename Base::PeersMap;
    using typename Base::Packet;

  public:
    using Base::parent, Base::peers, Base::reactor;
    using Base::Base;

    explicit BasicSocketService(Parent* parent, const Endpoint& remote)
    :  Base(parent->reactor())
    ,  remote_(remote) {}

    void open() {
        socket_.open(*reactor(), remote_.protocol());
        socket_.bind(remote_);
    }

    /// @see SocketRef
    void close() { socket_.close(); } 

    Socket& socket() { return socket_; }

    /// emplace peer 
    Peer* emplace(Peer&& peer) {
        auto it = peers().find(remote_);
        if(it==peers().end()) {
            return Base::emplace(std::move(peer));
        }
        return nullptr;
    }

    void run() {
        if constexpr(tb::SocketTraits::has_accept<Socket>) { // tcp or something like that
            socket().async_accept(remote_, tb::bind([this](ClientSocket&& socket, std::error_code ec) {
                //parent()->newpeer_(tb::ip_endpoint(remote_), ec);
                if(!ec) {
                    auto* peer = emplace(Peer(std::move(socket), remote_, self()));
                    if(peer) {
                        parent()->async_accept(peer, tb::bind([peer](std::error_code ec) {
                            auto* s = static_cast<Self*>(peer->parent());
                            s->on_accept(peer, ec);
                        }));
                    }
                } else {
                    on_error(ec);
                }
            }));
        } else { // udp
            auto buf = rbuf_.prepare(self()->buffer_size());
            socket().async_recvfrom(buf, 0, remote_, tb::bind([this](ssize_t size, std::error_code ec) {
                rbuf_.commit(size);
                if(!ec) {
                    auto socket_ref = boost::ref(socket());
                    auto *peer = emplace(Peer(std::move(socket_ref), remote_, self()));
                    if(peer) {
                        // patch packet buffer
                        peer->packet().buffer() = socket().rbuf();
                        peer->packet().header().src() = remote_;
                        parent()->async_accept(*peer, tb::bind([peer](std::error_code ec) {
                            auto* s = static_cast<Self*>(peer->parent());
                            std::size_t size = peer->packet().buffer().size();
                            s->rbuf_.consume(size); // FIXME: move rbuf to peer?
                            s->on_accept(*peer, ec);
                        }));
                    };
                }
            }));
        }
    }
   protected:
    constexpr std::size_t buffer_size() const { return 4096; }
    void on_accept(Peer& peer, std::error_code ec) {
        if(!ec) {
            self()->run();
        } else {
            on_error(ec);
        }
    }
    void on_error(std::error_code ec) {
        TOOLBOX_ERROR << "error "<<ec<<" acceptor remote="<<remote_;
    }
  protected:
    Socket socket_;
    Endpoint remote_;
    tb::Buffer rbuf_;
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
, class RoutingStrategyT
, typename StateT
>
class BasicServer : public io::BasicService<Self, typename PeerT::Reactor, StateT, core::Component>
{
    FT_MIXIN(Self);
    using Base = io::BasicService<Self, typename PeerT::Reactor, StateT, core::Component>;
  public:
    using Service = io::SocketService<PeerT, typename PeerT::Socket, StateT, Self>;
    using Peer = PeerT;
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
            auto [it, is_new] = services_.emplace(ep, Service(self(), ep));
        }
        if(was_open)
            open();
    }

    void open() {
        for(auto& [ep, acpt]: services_) {
            acpt.open();
        }
        Base::open();
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

    void close() {
        Base::close(); // peers

        for(auto& [ep, acpt]: services_) {
            acpt.close();
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