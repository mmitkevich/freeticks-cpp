#pragma once
#include "ft/core/Component.hpp"
#include "ft/core/Identifiable.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/io/Routing.hpp"
#include "toolbox/io/Socket.hpp"
#include "toolbox/net/Endpoint.hpp"
#include "toolbox/net/Sock.hpp"
#include "toolbox/util/Slot.hpp"
#include <ft/io/Service.hpp>
#include <type_traits>
#include "ft/utils/Compat.hpp"
#include "ft/core/Server.hpp"

namespace ft::io {

template<class Self, class PeerT, class ServerSocketT, typename StateT, class ParentT, typename...ArgsT>
class BasicSocketService : public EnableParent<ParentT, BasicPeerService<Self, PeerT, StateT, ArgsT...>>
{
    FT_SELF(Self);
    using Base = EnableParent<ParentT, BasicPeerService<Self, PeerT, StateT, ArgsT...>>;
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
    using Base::parent, Base::peers, Base::peers_, Base::reactor
        , Base::make_peer, Base::emplace_peer
        , Base::local;
    using Base::Base;

    BasicSocketService(const BasicSocketService& rhs)=delete;
    BasicSocketService& operator=(const BasicSocketService& rhs)=delete;

    BasicSocketService(BasicSocketService&& rhs)=default;
    BasicSocketService& operator=(BasicSocketService&& rhs)=default;

    explicit BasicSocketService(const Endpoint& ep, Parent* parent)
    :  Base(ep, parent->reactor(), parent)
    {}

    void open() {
        socket_.open(*reactor(), local().protocol());
        socket_.bind(local());
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
                            on_error(next_peer(), ec, "not accepted", TOOLBOX_FILE_LINE);
                        }
                    }));
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
            resume_ = done; // save continuation
            parent()->async_accept(peer, tb::bind(
            [p=&peer](std::error_code ec) {
                auto* self = static_cast<Self*>(p->parent());                
                if(!ec) {
                    self->parent()->async_handle(*p, p->packet(), self->resume_.release());
                } else {
                    self->on_error(*p, ec,  "not accepted", TOOLBOX_FILE_LINE);
                }
            }));
        }
    }
    void on_error(Peer& peer, std::error_code ec, const char* what="error", const char* loc="") {
        TOOLBOX_ERROR << loc << what <<", ec:"<<ec<<", peer:"<<peer.remote();
    }
  protected:
    ServerSocket socket_;
    tb::DoneSlot resume_;
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
, typename StateT
>
class BasicServer : public io::BasicService<Self, typename PeerT::Reactor, StateT, core::Component>
{
    FT_SELF(Self);
    using Base = io::BasicService<Self, typename PeerT::Reactor, StateT, core::Component>;
  public:
    using Service = io::SocketService<PeerT, ServerSocketT, StateT, Self>;
    using Packet = typename Service::Packet;
    using Peer = PeerT;
    using ServerSocket = ServerSocketT;
    using Reactor = typename Peer::Reactor;
    using Endpoint = typename Peer::Endpoint;
    using Protocol = ProtocolT;
    using ServicesMap = tb::unordered_map<Endpoint, std::unique_ptr<Service>>;    
  public:
    using Base::Base;
    using Base::parameters;
    using Base::reactor;
    using Base::is_open;
    using Base::open, Base::close;

    template<typename T>
    class Stream : public core::Stream::BasicSlot<Stream<T>, const T&, tb::SizeSlot> {
      public:
        Stream(Self* parent)
        : parent_(parent) {}

        void invoke(const T&e, tb::SizeSlot done) {
            parent_->async_write(e, done);
        }
      protected:
        Self* parent_;
    };

    class PeersMap {
        using ServicesIterator = typename ServicesMap::iterator;
        using PeersIterator = typename Service::PeersMap::iterator;
      public:
        using key_type = decltype(std::declval<PeersIterator>()->first);
        using mapped_type = decltype(std::declval<PeersIterator>()->second);
        
        PeersMap(ServicesIterator begin, ServicesIterator end)
        : begin_(begin)
        , end_(end) {}

        class iterator {
          public:
            iterator() = default;
            iterator(ServicesIterator svc_it, ServicesIterator svc_end)
            : svc_it_(svc_it)
            , svc_end_(svc_end)
            { 
                if(svc_it_!=svc_end_) {
                    maybe_peer_it_ = peers().begin();
                }
            }
            typename Service::PeersMap& peers() {
                return svc_it_->second->peers();
            }

            auto& operator*() { return *maybe_peer_it_.value(); }
            auto* operator->() { return &(*maybe_peer_it_.value()); }
            auto& operator++() { advance(); return *this; }
            //auto& operator++(int) { advance(); return *this;}
            friend bool operator==(const iterator& lhs, const iterator& rhs) {
                return lhs.svc_it_ == rhs.svc_it_ && lhs.maybe_peer_it_ == rhs.maybe_peer_it_;
            }
            friend bool operator!=(const iterator& lhs, const iterator& rhs) {
                return !operator==(lhs, rhs);
            }

            void advance() {
                if(maybe_peer_it_) {
                    ++maybe_peer_it_.value();
                    if(maybe_peer_it_.value()==peers().end()) {
                        maybe_peer_it_.reset();
                        do {
                            ++svc_it_;
                            if(svc_it_!=svc_end_)
                                maybe_peer_it_ = peers().begin();
                            else 
                                break;
                        } while(!maybe_peer_it_);
                    }
                }
            }
          private:
            ServicesIterator svc_it_;
            ServicesIterator svc_end_;            
            std::optional<PeersIterator> maybe_peer_it_;
        };
        auto begin() const { return iterator(begin_, end_); }
        auto end() const { return iterator(begin_, end_); }
      private:
        ServicesIterator begin_, end_;
    };
    
    //using tt = tb::SocketTraits::sock_async_accept_t<ServerSocket>;
    const ServicesMap& services() { return services_; }

    const PeersMap peers() const { return PeersMap(services_.begin(), services_.end()); }
    PeersMap peers() { return PeersMap(services_.begin(), services_.end()); }

    Protocol& protocol() { return protocol_; }

    void on_parameters_updated(const core::Parameters& params) {
        bool was_open = is_open();
        if(was_open)
            close();
        assert(services_.empty());
        for(auto conn_p: parameters()["connections"]) {
            auto iface = conn_p.value_or("interface", std::string{});
            auto ep = tb::TypeTraits<Endpoint>::from_string(iface);
            auto [it, is_new] = services_.emplace(ep, make_service(ep));
        }
        if(was_open)
            open();
    }

    std::unique_ptr<Service> make_service(const Endpoint& ep) {
        return std::unique_ptr<Service>(new Service(ep, self()));
    }

    template<typename Fn>
    int for_each_service(Fn fn) {
        for(auto& it: services_) {
            auto& svc=it.second;
            fn(*svc);
        }        
        return services_.size();
    }

    void do_open() {
        for_each_service([](auto& svc) {
            svc.open();
        });
        Base::do_open();
        protocol_.open();    // change state
        run();
    }
    
    /// accepts clients, reads messages from them and apply protocol logic
    void run() {
        TOOLBOX_DUMPV(5)<<"#services:"<<services_.size();
        for_each_service([](auto& svc) {
            svc.run();
        });

    }

    /// called when service accepts peer
    void async_accept(Peer& peer, tb::DoneSlot done) {
        TOOLBOX_DEBUG<<"new_peer remote:"<<peer.remote();
        newpeer_(peer.id(), tb::ip_endpoint(peer.remote()));
        done({});
    }

    void do_close() {
        protocol_.close();
        for_each_service([](auto& svc) {
            svc.close();
        });
        services_.clear();
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

    void async_handle(Peer& peer, const Packet& packet, tb::DoneSlot done) {
        TOOLBOX_DEBUG<<"received from: "<<peer.remote()<<", size:"<<packet.buffer().size();
        protocol_.async_handle(peer, packet, done);
    }

    template<typename MessageT>
    void async_write(Peer& peer, const MessageT& message, tb::SizeSlot done) {
        protocol_.async_write(peer, message, done);
    }

    NewPeerSignal& newpeer() { return newpeer_; }

  protected:
    Protocol protocol_;
    ServicesMap services_; // indexed by local endpoint
    Endpoint remote_;
    NewPeerSignal newpeer_;
}; // BasicService

} // ft::io