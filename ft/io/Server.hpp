#pragma once
#include "ft/core/Component.hpp"
#include "ft/core/Identifiable.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/Stream.hpp"
#include "toolbox/io/Socket.hpp"
#include "toolbox/net/Endpoint.hpp"
#include "toolbox/net/Sock.hpp"
#include "toolbox/util/Slot.hpp"
#include <ft/io/Service.hpp>
#include <stdexcept>
#include <type_traits>
#include "ft/utils/Compat.hpp"
#include "ft/core/Server.hpp"

namespace ft::io {


class PeerService: public io::Service {
    using Base = io::Service;
public:
    using Base::Base;
    NewPeerSignal& newpeer() { return newpeer_; }
protected:
    NewPeerSignal newpeer_;
};

/// Acceptor
template<class Self, class PeerT, class ServerSocketT, typename...O>
class BasicServer : public BasicPeerService<Self, PeerT, O...>
{
    FT_SELF(Self);
    using Base = BasicPeerService<Self, PeerT, O...>;
  public:
    using typename Base::Parent;
    using ServerSocket = ServerSocketT;
    
    static constexpr bool has_socket_accept() { return tb::SocketTraits::has_accept<ServerSocket>; }    

    using ClientSocket = typename ServerSocket::ClientSocket;
    using typename Base::Endpoint;
    using typename Base::Peer;
    using typename Base::PeersMap;
    using typename Base::Packet;
    using typename Base::Reactor;
  public:
    using Base::peers, Base::peers_, Base::reactor
        , Base::make_peer, Base::emplace_peer
        , Base::local, Base::async_write;
    using Base::Base;

    explicit BasicServer(const Endpoint& ep, Reactor* reactor=nullptr, io::PeerService* parent=nullptr)
    :  Base(ep, reactor, parent)
    {}

    void open() {
        socket_.open(reactor(), local().protocol());
        socket_.bind(local());
    }

    PeerService* parent() {
        return static_cast<PeerService*>(Base::parent());
    }
    using Base::parent; // setter

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
        TOOLBOX_DUMPV(5)<<"self:"<<self()<<", local:"<<local();
        if constexpr(has_socket_accept()) {
            return make_peer(ClientSocket());
        } else {
            return make_peer(std::ref(socket()));
        }
    }

    void run() {
        TOOLBOX_DUMPV(5)<<"self:"<<self()<<", local:"<<local();
        assert(static_cast<Self*>(next_peer_->parent())==self());
        // next possible peer
        if constexpr(has_socket_accept()) { // tcp
            socket().async_accept(next_peer().remote(), tb::bind(
            [this](ClientSocket&& socket, std::error_code ec) {
                if(!ec) {
                    next_peer().socket(std::move(socket));
                    parent()->newpeer()(next_peer().id(), ec);
                    if(!ec) {
                        next_peer().run([](Peer& peer, Packet& packet, tb::DoneSlot done) {
                            auto* self = static_cast<Self*>(peer.parent());
                            self->parent()->async_handle(peer, packet, done);
                        });    // peer's loop
                        run();    // accept again
                    } else {
                        on_error(next_peer(), ec, "not accepted", TOOLBOX_FILE_LINE);
                    }
                } else {
                    on_error(next_peer(), ec, "not accepted", TOOLBOX_FILE_LINE);
                }
            }));
        } else { // udp
            next_peer().socket(std::move(std::ref(socket()))); // just ref to our socket
            struct Handler {
                void operator()(Peer& peer, Packet& packet, tb::DoneSlot done) {
                    auto* self = static_cast<Self*>(peer.parent());                    
                    TOOLBOX_DUMPV(5)<<"self="<<self<<", peer="<<&peer;
                    self->async_accept_and_handle(peer, done);
                }
            };
            next_peer().template run<Handler>(); // peer's loop
        }
    }
    void async_accept_and_handle(Peer& peer, tb::DoneSlot done) {
        TOOLBOX_DUMPV(5)<<"self:"<<self()<<" peer:"<<&peer<<",local:"<<peer.local()<<",remote:"<<peer.remote()<<", peer_id:"<<peer.id()<<", #peers:"<<peers_.size();
        auto it = peers_.find(peer.id());
        if(it==peers_.end()) {
            parent()->newpeer()(peer.id(), {});
            self()->async_handle(peer, peer.packet(), done);
        }
    }

    void on_error(Peer& peer, std::error_code ec, const char* what="error", const char* loc="") {
        TOOLBOX_ERROR << loc << what <<", ec:"<<ec<<", peer:"<<peer.remote();
    }
  protected:
    ServerSocket socket_;
    std::unique_ptr<Peer> next_peer_ = make_next_peer();
};

template<class PeerT, class ServerSocketT>
class Server: public BasicServer<Server<PeerT, ServerSocketT>, PeerT, ServerSocketT> 
{
    using Base = BasicServer<Server<PeerT, ServerSocketT>, PeerT, ServerSocketT>;
  public:
    using Base::Base;
};


/// multiple services
template<class Self, class ServicesL, typename...O>
class BasicMultiServer : public io::BasicMultiService<Self, PeerService, ServicesL, O...>
{
    FT_SELF(Self);
    using Base = io::BasicMultiService<Self, PeerService, ServicesL, O...>;
  public:
    using Base::Base;
    using Base::parameters;
    using Base::open, Base::close, Base::is_open, Base::service, Base::for_each_service, Base::reactor;

    void on_parameters_updated(const core::Parameters& params) {
        bool was_open = is_open();
        if(was_open)
            close();
        for(auto conn_p: parameters()["connections"]) {
            auto ep = conn_p.strv("local");
            auto proto = conn_p.strv("transport");
            self()->get_service(proto, ep, reactor()); // will create service
        }
        if(was_open)
            open();
    }

    /// route via all services
    template<typename MessageT>
    void async_write(const MessageT& m, tb::SizeSlot done) {
        write_.set_slot(done);
        write_.pending(0);
        for_each_service([&](auto& svc) {
            write_.inc_pending();
            svc.async_write(m, write_.get_slot());
        });
    }

    void do_open() {
        Base::do_open(); // will open services
        self()->run();
    }
    
    /// accepts clients, reads messages from them and apply protocol logic
    void run() {
        for_each_service([](auto& svc) {
            svc.run();
        });
    }

    void shutdown(PeerId id) {
        for_each_service([id](auto& svc) {
            svc.shutdown(id);
        });
    }

    template<typename Fn>
    int for_each_peer(const Fn& fn) {
      // decide who wants this tick and broadcast
      int count = 0;
      for_each_service([&](auto& svc) {
        count += svc.for_each_peer(fn);
      });
      return count;
    }

    core::Stream& slot(core::StreamTopic topic) {
        throw std::logic_error("notimpl");
    }
    core::Stream& signal(core::StreamTopic topic) {
        throw std::logic_error("notimpl");
    }
    core::SubscriptionSignal& subscription() {
        return subscription_;
    }
protected:
    tb::PendingSlot<ssize_t, std::error_code> write_;
    core::SubscriptionSignal subscription_;

};

template<class ServicesL>
class MultiServer: public BasicMultiServer<MultiServer<ServicesL>, ServicesL> 
{
    using Base = BasicMultiServer<MultiServer<ServicesL>, ServicesL>;
  public:
    using Base::Base;
};

} // ft::io