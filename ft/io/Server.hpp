#pragma once
#include "ft/core/Component.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/io/Router.hpp"
#include "toolbox/io/Socket.hpp"
#include "toolbox/util/Slot.hpp"
#include <ft/io/Service.hpp>

namespace ft::io {



/// multiple connected remote peers - single server socket
template<typename DerivedT, typename ProtocolT, typename PeerT, typename ServerSocketT>
class BasicServer :  public io::BasicPeersService<DerivedT, ProtocolT, PeerT, core::State>
{
    using This = BasicServer<DerivedT, ProtocolT, PeerT, ServerSocketT>;
    using Base = io::BasicPeersService<DerivedT, ProtocolT, PeerT, core::State>;
    using Self = DerivedT;
    Self* self() { return static_cast<Self*>(this); }
    const Self* self() const { return static_cast<Self*>(this); }
public:
    using ServerSocket = ServerSocketT;
    using ClientSocket = typename ServerSocket::ClientSocket;
    using typename Base::Peer;
    using typename Base::Reactor;
    using typename Base::Endpoint;
    using typename Base::Protocol;
    using typename Base::Transport;
    using typename Base::PeersMap;
    using typename Base::Packet;
    class Serv  {
      public:
        Serv() = default;
        
        Serv(Serv&& rhs) = default;
        Serv& operator=(Serv&& rhs) = default;

        Serv(const Serv&) = delete;
        Serv& operator=(Serv const & rhs) = delete;

        explicit Serv(Self* self)
        : service_(self)
        {}

        Self* service() { return service_; }
        ServerSocket& socket() { return socket_; }
      protected:
        Self* service_{};
        ServerSocket socket_;
    };
    using ServiceMap = tb::unordered_map<Endpoint, Serv>;    
public:
    using Base::Base;
    
    using Base::parameters, Base::reactor, Base::transport, Base::is_open;
    using Base::peers, Base::async_write, Base::async_read, Base::async_drain, Base::emplace;
    
    //using tt = tb::SocketTraits::sock_async_accept_t<ServerSocket>;
    const ServiceMap& services() { return services_; }

    void on_parameters_updated(const core::Parameters& params) {
        bool was_open = is_open();
        if(was_open)
            close();
        services_.clear();            
        for(auto conn_p: parameters()["connections"]) {
            auto iface = conn_p.value_or("interface", std::string{});
            auto ep = tb::TypeTraits<Endpoint>::from_string(iface);
            services_.emplace(ep, Serv(self()));
        }
        if(was_open)
            open();
    }

    void open() {
        for(auto& [ep, serv]: services_) {
            auto& s = serv.socket();
            Reactor* r = reactor();
            s.open(*r, transport());
            s.bind(ep);
        }
        Base::open();        
    }

    void async_accept(tb::DoneSlot done) {
        accept_.set_slot(done);        
        if constexpr(tb::SocketTraits::has_async_accept<ServerSocket>) { // tcp
            for(auto& [ep, serv]: services_) {
                serv.async_accept(remote_, [this](ClientSocket&& sock, std::error_code ec) {
                    auto& peer = emplace(Peer(self(), std::move(sock), std::move(remote_)));
                    async_drain(peer, tb::bind<&Self::on_drain>(self()));   // start draining
                    accept_.notify({}); 
                });
            }
        } else { // udp
           for(auto& [ep, serv]: services_) {
                auto buf = self()->rbuf().prepare(self()->buffer_size());
                serv.socket().async_recvfrom(buf, 0, remote_, tb::bind([s=&serv](ssize_t size, std::error_code ec) {
                    auto* self = static_cast<Self*>(s->service());
                    if(!ec) {
                        self->maybe_new_peer(*s, std::move(self->remote_), size);
                    }
                }));
           }
        }
    }

    void maybe_new_peer(Serv& serv, Endpoint&& remote, ssize_t size) {
        // got something, need to check if got new peer
        auto it = peers().find(remote);
        auto& peer = (it==peers().end()) ? 
                        emplace(Peer(self(), ClientSocket(&serv.socket()), std::move(remote))) // new peer
                        : it->second; // old peer
        self()->async_process(peer, size, tb::bind([&peer](std::error_code ec) {
            static_cast<Self*>(peer.service())->on_process(peer, peer.packet(), ec);
        }));
    }

    void on_process(Peer& peer, Packet& packet, std::error_code ec) {
        auto rsize = packet.buffer().size(); 
        self()->rbuf().consume(rsize); // size from packet
        self()->async_drain(peer, tb::bind<&Self::on_drain>(self()));
    }

    void on_drain(Peer& peer, std::error_code ec) {
        if(ec) {
            TOOLBOX_INFO<<"Drain failed "<<ec;
        } else {
            // continue
            async_drain(peer, tb::bind<&Self::on_drain>(self()));
        }
    }

    void close() {
        Base::close(); // peers

        for(auto& [ep, serv]: services_) {
            serv.socket().close();
        }
    }    

    template<typename MessageT>
    void async_publish(const MessageT& message, tb::SizeSlot done) {
      // decide who wants this tick and broadcast
      for(auto& [ep, peer]: Base::peers_) {
        auto topic = message.topic();
        if(peer.subscription().test(topic)) {
          async_write(peer, message, done);
        }
      }
    }

protected:
    ServiceMap services_; // local endpoints
    Endpoint remote_;
    tb::CompletionSlot<std::error_code> accept_;
};

} // ft::io