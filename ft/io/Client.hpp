#pragma once

#include "ft/utils/Common.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/Component.hpp"
#include "toolbox/io/Reactor.hpp"
#include "toolbox/util/RobinHood.hpp"
#include "toolbox/util/Slot.hpp"
#include <boost/mp11/algorithm.hpp>
#include <sstream>
#include <system_error>
#include <toolbox/io/Socket.hpp>
#include <ft/utils/StringUtils.hpp>
#include "ft/core/Requests.hpp"
#include "ft/io/Service.hpp"
#include <deque>

namespace ft::io {

template<class RequestT>
using Response_T = typename RequestT::Response;

/// Idle timer mixin
template<typename Self>
class BasicIdleTimer
{
    FT_SELF(Self)
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

    void on_parameters_updated(const core::Parameters& params) {}

    void on_idle() { TOOLBOX_DUMP << "on_idle"; }
protected:
    tb::Timer idle_timer_;
}; // IdleTimer

template<class Self, class PeerT>
class BasicClient: public BasicPeerService<Self, PeerT>
{
    FT_SELF(Self);
    using Base = BasicPeerService<Self, PeerT>;
  public:
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
            Base::peers, Base::make_peer, Base::emplace_peer,
            Base::close, Base::open, Base::is_open;

    void on_parameters_updated(const core::Parameters& params) {
        auto was_open = is_open();
        if(was_open)
            close();

        self()->populate(params);
        
        if(was_open)
            open();
    }

    void populate(const core::Parameters& params) {
        assert(Base::peers_.empty());
        std::string transport = params.str("transport");
        auto conns_pa = params["connections"];
        for(auto p: conns_pa) {
            std::string_view type = p.str("type");
            auto iface = p.str("local","");
            if(Transport::name() == transport) {
                for(auto e : p["remote"]) {
                    std::string url {e.get_string()};
                    if(!iface.empty())
                        url = url+"|interface="+iface;
                    emplace_peer(make_peer(url));
                }
            }
        }
    }

    // overrides states transition
    void open() {
        state(State::PendingOpen);
        Base::do_open();
        self()->async_connect(tb::bind<&Self::on_connected>(self()));
    }
    
    void do_close() {
        Base::do_close();
    }

    void on_connected(std::error_code ec) {
        TOOLBOX_INFO << "connected to "<<Base::peers_.size()<<" peers, ec "<<ec;
        state(State::Open); // notifies on state changed
    }
 
    // connect all peers. Notifies when all done
    void async_connect(tb::DoneSlot done) {
        assert(connect_.empty());
        int pending = 0;
        Base::for_each_peer([this](auto& peer) {
            if(!peer.is_open() && !peer.is_connecting()) {
                peer.async_connect(tb::bind(
                [&peer](std::error_code ec) {
                    Self* self = static_cast<Self*>(peer.parent());
                    self->on_peer_connected(peer, ec); // after connect
                    self->connect_(ec); // calls done when all done
                }));
            }
        });
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
           self()->async_handshake(peer, [&peer](std::error_code ec) {
                struct fn { void operator()(Peer& peer, Packet& packet, tb::DoneSlot done) {
                    Self* self = static_cast<Self*>(peer.parent());
                    self->async_handle(peer, packet, done);
                }};
                peer.template run<fn>();
            });
        } else {
            on_error(peer, ec, "connect");
        }
    }
    
    void on_error(Peer& peer, std::error_code ec, const char* what="") {
        TOOLBOX_INFO<<what<<" peer "<<peer.remote()<<" ec "<<ec;
    }
 
  protected:
    tb::PendingSlot<std::error_code>  connect_;    
    tb::Signal<> connected_;
}; // BasicClient




/// multiple services
template<class Self, class ClientsL, typename...O>
class BasicMultiClient : public io::BasicMultiService<Self, io::Service, ClientsL, O...>
{
    FT_SELF(Self);
    using Base = io::BasicMultiService<Self, io::Service, ClientsL, O...>;
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
class MultiClient: public BasicMultiClient<MultiClient<ServicesL>, ServicesL> 
{
    using Base = BasicMultiClient<MultiClient<ServicesL>, ServicesL>;
  public:
    using Base::Base;
};
} // namespace ft::io