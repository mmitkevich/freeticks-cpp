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


/// Acceptor
template<class Self, class PeerT, class ServerSocketT, typename...O>
class BasicServer : public BasicPeerService<Self, PeerT, O...>
{
    FT_SELF(Self);
    using Base = BasicPeerService<Self, PeerT, O...>;
  public:
    using typename Base::Parent;
    using typename Base::Endpoint;
    using typename Base::Peer;
    using typename Base::PeersMap;
    using typename Base::Packet;
    using typename Base::Reactor;
    
    using ServerSocket = ServerSocketT;
    using ClientSocket = typename ServerSocket::ClientSocket;

    struct Acceptor  {
        Acceptor(Self* self)
        : self_(self)
        {
            TOOLBOX_DUMPV(5)<<"Acceptor this="<<this<<" self="<<self_;
        }

        Self* self() { return self_; }
        Peer& next_peer() { assert(next_peer_); return *next_peer_; }

        template<class Reactor>    
        void open(Reactor r) {
            socket_.open(r, local().protocol());
            socket_.bind(local());
        }

        void close() {
            socket_.close();
        }

        void configure(const core::Parameters& params) {
            auto iface = params.str("local","");
            local_ = tb::parse_ip_endpoint<Endpoint>(iface);
            next_peer_ = make_next_peer();
        }

        void async_accept() {
            TOOLBOX_DUMPV(5)<<"self:"<<self()<<", local:"<<local();
            assert(static_cast<Self*>(next_peer_->parent())==self());
            // next possible peer
            if constexpr(has_accept()) { // tcp
                async_accept(next_peer().remote(), tb::bind(
                [this](ClientSocket&& socket, std::error_code ec) {
                    if(!ec) {
                        next_peer().socket(std::move(socket));
                        self()->parent()->newpeer()(next_peer().id(), ec);
                        if(!ec) {
                            next_peer().run([](Peer& peer, Packet& packet, tb::DoneSlot done) {
                                auto* self = static_cast<Self*>(peer.parent());
                                self->parent()->async_handle(peer, packet, done);
                            });    // peer's loop
                            self()->async_accept();    // accept again
                        } else {
                            self()->on_error(next_peer(), ec, "not accepted", TOOLBOX_FILE_LINE);
                        }
                    } else {
                        self()->on_error(next_peer(), ec, "not accepted", TOOLBOX_FILE_LINE);
                    }
                }));
            } else { // udp
                next_peer().socket(std::move(std::ref(socket_))); // just ref to our socket
                struct Handler {
                    void operator()(Peer& peer, Packet& packet, tb::DoneSlot done) {
                        auto* self = static_cast<Self*>(peer.parent());                    
                        TOOLBOX_DUMPV(5)<<"self="<<self<<", peer="<<&peer;
                        self->async_accept_first(peer, done);
                    }
                };
                next_peer().template async_recv<Handler>(); // peer's loop
            }
        }

        Peer* emplace_next_peer() {
            Peer* peer = next_peer_.get();
            assert(peer->parent()==self());
            self()->emplace_peer(std::move(next_peer_));
            next_peer_ = make_next_peer();
            return peer;
        }
        
        auto make_next_peer() {
            TOOLBOX_DUMPV(5)<<"self:"<<self()<<", local:"<<local();
            if constexpr(has_accept()) {
                return self()->make_peer(ClientSocket(), local());
            } else {
                return self()->make_peer(std::ref(socket_), local());
            }
        }
        Endpoint& local() { return local_; }
        const Endpoint& local() const { return local_; }
        void local(const Endpoint& ep) { local_ = ep; }
    protected:
        Self* self_{};    
        Endpoint local_;    
        ServerSocket socket_;
        std::unique_ptr<Peer> next_peer_ {};
        static constexpr bool has_accept() { return tb::SocketTraits::has_accept<ServerSocket>; }    
    };

    using Acceptors = std::vector<Acceptor>;
  public:
    using Base::peers, Base::peers_, Base::reactor
        , Base::make_peer, Base::emplace_peer
        , Base::async_write;
    using Base::Base;

    using Base::do_open;

    void configure(const core::Parameters& params) {
        //assert(peers().empty());
        std::string transport = params.str("transport");
        for(auto pa: params["endpoints"]) {
            auto& acpt = acceptors_.emplace_back(self());
            acpt.configure(pa);
        }
    }
    
    void do_open() {
        Base::do_open();
        for(auto& acpt: acceptors_) {
            acpt.open(reactor());
            acpt.async_accept();
        }
    }

    PeerService* parent() {
        return static_cast<PeerService*>(Base::parent());
    }

    using Base::parent; // setter

    /// @see SocketRef
    void do_close() { 
        for(auto& acpt: acceptors_) {
            acpt.close();
        }
    } 

    Acceptors& acceptors() { return acceptors_; }


    void async_accept_first(Peer& peer, tb::DoneSlot done) {
        TOOLBOX_DUMPV(5)<<"self:"<<self()<<" peer:"<<&peer<<",local:"<<peer.local()<<",remote:"<<peer.remote()<<", peer_id:"<<peer.id()<<", #peers:"<<peers_.size();
        auto it = peers_.find(peer.id());
        if(it==peers_.end()) {
            self()->newpeer()(peer.id(), {});
        }
        self()->async_handle(peer, peer.packet(), done);        
    }

    void on_error(Peer& peer, std::error_code ec, const char* what="error", const char* loc="") {
        TOOLBOX_ERROR << loc << what <<", ec:"<<ec<<", peer:"<<peer.remote();
    }
  protected:
    std::vector<Acceptor> acceptors_;
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
        for(auto conn_p: parameters()["endpoints"]) {
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
        self()->async_accept();
    }
    
    /// accepts clients, reads messages from them and apply protocol logic
    void async_accept() {
        for_each_service([](auto& svc) {
            svc.async_accept();
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