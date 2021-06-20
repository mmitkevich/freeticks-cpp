#pragma once

#include "ft/core/Component.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/Service.hpp"
#include "ft/core/State.hpp"
#include "ft/core/Stream.hpp"
#include "ft/io/Protocol.hpp"
#include "toolbox/io/Reactor.hpp"
#include "toolbox/io/Runner.hpp"
#include "toolbox/io/Socket.hpp"
#include "toolbox/util/Slot.hpp"
#include "toolbox/util/TypeTraits.hpp"
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/tuple.hpp>
#include <exception>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>

namespace ft::io {

using Reactor = toolbox::os::Reactor;


Reactor* current_reactor();

class Service: public core::Component, public core::BasicStateful<core::State> {
    using Stateful = core::BasicStateful<core::State>;
    using Base = core::Component;
public:
    using Reactor = tb::Reactor;   // forget class here
    using Stateful::state;
public:
    using Component::Component;
    Service(Reactor* reactor, core::Component* parent=nullptr, core::Identifier id={})
    : Base(parent, id)
    , reactor_(reactor) {}
    void open() {}
    void close() {}
    void configure(const core::Parameters& params) {}
    std::string_view protocol() { return protocol_; }
    void protocol(std::string_view val) { protocol_ = val; }
    Reactor* reactor() { return reactor_; }
protected:
    std::string_view protocol_;
    Reactor* reactor_{};
};

template<class Self, class Base, typename...>
class BasicService: public Base
, public core::BasicParameterized<Self>
{
    FT_SELF(Self);
  public:
    using typename Base::State;
    using typename Base::Reactor;
    using Base::state;
    
    explicit BasicService(Reactor* reactor, Component* parent=nullptr, Identifier id={})
    : Base(reactor, parent, id) {
        if constexpr(TB_IS_VALID(self(), self()->Protocol)) {
            Base::protocol(self()->Protocol);
        }
        TOOLBOX_DUMPV(5)<<"Service::ctor, proto:"<<self()->protocol()<<" self:"<<self()<<", parent(Component): "<<parent<<", id:"<<id;
    }

    // start component, throws only std::runtime_error
    void start() {
        try {
            self()->open();
        } catch(std::exception& e) {
            TOOLBOX_ERROR<<" Component "<<Base::id()<<" Failed with error: "<<e.what();            
            state(State::Failed);
            throw std::runtime_error(e.what());
        }
#ifndef FT_NO_CATCH_ALL
        catch(...) {
            state(State::Crashed);
            throw std::runtime_error("crashed");
        }
#endif
    }

    void open() {
        state(State::PendingOpen);
        self()->do_open();
    }

    void do_open() { 
        state(State::Open);
    }
    
    void on_parameters_updated(const core::Parameters& params) {
        auto was_open = self()->is_open();
        if(was_open)
            self()->close();

        self()->configure(params);
        
        if(was_open)
            self()->open();
    }
    
    void configure(const core::Parameters& params) {
        Base::configure(params);
    }

    /// stops, throws only std::runtime_error
    void stop() {
        try {
            self()->close();
        } catch(std::exception& e) {
            state(State::Failed);
            throw  std::runtime_error(e.what());
        }
#ifndef FT_NO_CATCH_ALL
        catch(...) {
            state(State::Crashed);
            throw std::runtime_error("crashed");
        }
#endif
    }

    void close() {
        state(State::PendingClosed);
        self()->do_close();
        state(State::Closed);
    }

    void do_close() {}

    /// @returns true if Service is open
    bool is_open() const { return state()==State::Open; }
    
    /// @returns true if Service is opening 
    bool is_open_pending() const { return state()==State::PendingOpen; }

};


template<class Self, typename...O>
using BasicApp = io::BasicService<Self, io::Service, O...>;


class PeerService: public io::Service {
    using Base = io::Service;
public:
    using Base::Base;
    NewPeerSignal& newpeer() { return newpeer_; }
protected:
    NewPeerSignal newpeer_;
};

/// Adds PeersMap
template<class Self, class PeerT, typename...O>
class BasicPeerService : public BasicService<Self, io::PeerService, O...>
{
    FT_SELF(Self);
    using Base = BasicService<Self, io::PeerService, O...>;
  public:
    using typename Base::Reactor;
    using Peer = PeerT;
    using PeerPtr = std::unique_ptr<Peer>;
    using PeersMap = tb::unordered_map<PeerId, PeerPtr>;
    using Endpoint = typename Peer::Endpoint;
    using Packet = typename Peer::Packet;
    using Socket = typename Peer::Socket;
    using Transport = typename Peer::Transport;
  public:
    using typename Base::State;
    
    using Base::Base;


    // most basic configuration is to define local endpoint to listen on
    void configure(const core::Parameters& params) { }

    void do_open() {
        TOOLBOX_INFO << "opening "<< peers_.size() << " peers";
        for_each_peer([this](auto& peer) { 
            self()->do_open(peer);
        });
    }
    


    void do_open(Peer& peer) {
        peer.open(self()->reactor());
    }

    void do_close() {
        TOOLBOX_INFO << "closing "<< peers_.size() << " peers";
        for_each_peer([this](auto& peer) { 
            do_close(peer);
        });
    }

    void do_close(Peer& peer) {
        peer.close();
    }

    /// close & clear all peers
    void clear() {
        self()->close();
        peers_.clear();
    }

    template<typename ...ArgsT>
    auto make_peer(ArgsT... args) { 
        auto peer = PeerPtr(new Peer(std::forward<ArgsT>(args)..., self())); 
        return peer;
    };

    Peer* get_peer(PeerId id) {
        auto it = peers_.find(id);
        if(it!=peers_.end()) {
            return it->second;
        }
        return nullptr;
    }
    /// close and remove peer identified by endpoint
    /// @returns true if peer was found, false overwise
    bool shutdown(PeerId id) {
        auto it = peers_.find(id);
        if(it!=peers_.end()) {
            it->second->close();
            peers_.erase(it);
            return true;
        }
        return false;
    }

    /// @returns peers by endpoint
    const PeersMap& peers() const { return peers_; }
    PeersMap& peers() { return peers_; }  

    template<typename Fn>
    int for_each_peer(const Fn& fn) {
      for(auto& it: peers_) {
        auto& peer=it.second;
        fn(*peer);
      }
      return peers_.size();
    }
    Peer& emplace_peer(PeerPtr peer) {
        auto* ptr = peer.get();
        //auto ep = peer->remote();
        auto id = peer->id();
        assert(id);
        assert(peer);
        peers_[id] = std::move(peer);
        assert(peers_[id]!=nullptr);
        return *ptr;
    }
    template<typename MessageT>
    void async_write(const MessageT& m, tb::SizeSlot done) {
        std::size_t pending = 0;
        for_each_peer([&](auto& peer) {
            if(self()->route(peer, m.topic(), m.instrument_id())) {
                pending++;
            }
        });
        write_.set_slot(done);        
        write_.pending(pending);
        for_each_peer([&](auto& peer) {
             if(self()->route(peer, m.topic(), m.instrument_id())) {
              /// delegate to derived (e.g. Protocol)
              self()->async_write_to(peer, m, write_.get_slot());
            }
        });
    }

    /// route everythere by default
    bool route(Peer& peer, StreamTopic topic, InstrumentId instrument) {
        return true;
    }

  protected:
    PeersMap peers_;
    tb::PendingSlot<ssize_t, std::error_code> write_; // when write is done
};


/// TODO: Migrate to type-erased Proxy<IService>
template<class Self, class MultiServiceT, class ServicesL, typename...O>
class BasicMultiService : public io::BasicService<Self, MultiServiceT, O...> {
    using Base = io::BasicService<Self, MultiServiceT, O...>;
    FT_SELF(Self);
public:
    //using Base::Base;
    template<class ServiceT>
    using ServiceMapM = tb::unordered_map<typename ServiceT::Endpoint, std::unique_ptr<ServiceT>>;
    using ServicesTuple = mp::mp_rename<mp::mp_transform<ServiceMapM, ServicesL>, std::tuple>; /// tuple
    using Base::parent;
    using typename Base::Reactor;

    constexpr static std::string_view Protocol = "Multi";

    explicit BasicMultiService(Reactor* reactor=nullptr, Component* parent=nullptr, Identifier id={})
    : Base(reactor, parent, id) {
    }
    template<typename Fn>
    int for_each_service(Fn&& fn) {
        int count = 0;
        mp::tuple_for_each(services_, [&](auto& svcmap) {
            for(auto& it: svcmap) {
                if(it.second) {
                    fn(*it.second);
                    ++count;
                }
            }
        });
        return count;
    }
    const auto& services() { return services_; }

    template<class ServiceT, typename EndpointT, typename...ArgsT>
    std::unique_ptr<ServiceT> make_service(const EndpointT& endpoint, ArgsT...args) {
        auto svc =  std::make_unique<ServiceT>(endpoint, std::forward<ArgsT>(args)...);
        svc->parent(self());
        self()->on_new_service(*svc);
        return svc;
    }

    template<class ServiceT>
    void on_new_service(ServiceT& svc) {}

    template<class R>
    struct is_compatible {
        template<typename S>
        struct fn {
            using V = std::decay_t<decltype(*std::declval<S&>().begin()->second)>;
            constexpr static bool value = std::is_same_v<S, R>;
        };
    };

    template<class ServiceT, typename...ArgsT>
    ServiceT* service(const typename ServiceT::Endpoint& endpoint, ArgsT...args) {
        static_assert(!std::is_pointer_v<ServiceT>);
        //static_assert(mp::mp_count_if_q<ServicesTuple, is_compatible<ServiceT>>::value==1, "ServiceTuple does not contain ServiceT");
        ServiceT* result = nullptr;
        mp::tuple_for_each(services_, [&](auto& svcmap) {
            if constexpr(std::is_same_v<std::decay_t<decltype(*std::declval<decltype(svcmap)&>()[endpoint])>, ServiceT>) {
                auto it = svcmap.find(endpoint);
                if(it!=svcmap.end()) {
                    result = it->second.get();
                } else {
                    std::unique_ptr<ServiceT> ptr = self()->template make_service<ServiceT>(endpoint, std::forward<ArgsT>(args)...);
                    result = ptr.get();
                    svcmap.emplace(endpoint, std::move(ptr));
                }
            }
        });
        return result;
    }
    template<typename...ArgsT>
    Service* get_service(std::string_view protocol, std::string_view endpoint_str, ArgsT...args) {
        Service* result = nullptr;
        mp::tuple_for_each(services_, [&](auto& svcmap) {
            using EndpointT = std::decay_t<decltype(svcmap.begin()->first)>;
            using ServiceT = std::decay_t<decltype(*svcmap.begin()->second)>;
            if(EndpointT::protocol_type::name() == protocol) {
                auto endpoint  = tb::TypeTraits<EndpointT>::from_string(endpoint_str);
                auto it = svcmap.find(endpoint);
                if(it!=svcmap.end()) {
                    result = it->second.get();
                } else {
                    auto ptr = self()->template make_service<ServiceT>(endpoint, std::forward<ArgsT>(args)...);
                    result = ptr.get();
                    svcmap.emplace(endpoint, std::move(ptr));
                }
            }
        });
        assert(result);
        return result;
    }
    
    /// forward open
    void do_open() {
        Base::do_open();
        for_each_service([](auto& svc) {
            svc.open();
        });
    }
    
    /// forward close
    void do_close() {
        Base::do_close();        
        mp::tuple_for_each(services_, [](auto& svcmap) {
            svcmap.clear();
        });
    }

    template<class T>
    void write(const T& val) {
       service<T>()->write(val);
    }

    /// write
    template<class T, typename DoneT>
    void async_write(const T& val, DoneT done) {
       service<T>()->async_write(val, std::forward<DoneT>(done));
    }
protected:
    ServicesTuple services_;
};

} // ft::io