#pragma once

#include "ft/core/Component.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/Service.hpp"
#include "ft/core/State.hpp"
#include "ft/core/Stream.hpp"
#include "toolbox/io/Reactor.hpp"
#include "toolbox/io/Runner.hpp"
#include "ft/io/Routing.hpp"
#include "toolbox/util/Slot.hpp"
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/tuple.hpp>
#include <exception>
#include <sstream>
#include <stdexcept>
#include <type_traits>

namespace ft::io {

using Reactor = toolbox::io::Reactor;


Reactor* current_reactor();

/// Reactor-aware component
template<class ReactorT=io::Reactor>
class BasicReactive {
  public:
    using Reactor = ReactorT;  
    explicit BasicReactive(Reactor* reactor=nullptr)
    : reactor_(reactor) {}

    Reactor* reactor() { assert(reactor_); return reactor_; }
    const Reactor* reactor() const { assert(reactor_); return reactor_; }
  protected:
    Reactor* reactor_{};
};

template<class ReactorT=io::Reactor>
class BasicReactiveComponent: public Component, public BasicReactive<ReactorT> {
    using Base = core::Component;
    using Reactive = BasicReactive<ReactorT>;
    using Self = BasicReactiveComponent<ReactorT>;
public:
    using typename Reactive::Reactor;
    using Parent = Self;
    using Reactive::reactor;

    BasicReactiveComponent(Reactor* reactor=nullptr, core::Component* parent=nullptr, Identifier id={})
    : Base(parent, id)
    , Reactive(reactor) {}
    
    Self* parent() { return static_cast<Self*>(parent_);}
    const Self* parent() const { assert(parent_); return static_cast<const Self*>(parent_); }
    void parent(Self* val) { assert(val);  parent_ = val;}
};

/// Reactor-aware service with state
template<class Self, class ReactorT=io::Reactor, typename StateT=core::State, typename...>
class BasicService: public io::BasicReactiveComponent<ReactorT>
, public core::BasicParameterized<Self>
, public core::BasicStateful<StateT>
{
    FT_SELF(Self);
    using Base = io::BasicReactiveComponent<ReactorT>;
    using Stateful = core::BasicStateful<StateT>;
  public:
    using Reactor = ReactorT;
    using Stateful::state, Base::parent;
    using Base::reactor;
    using Ref = BasicRef<IService>;
    

    explicit BasicService(Reactor* reactor=nullptr, Component* parent=nullptr, Identifier id={})
    : Base(reactor, parent, id) {
        TOOLBOX_DUMPV(5)<<"Service::ctor, self:"<<self()<<", reactor:"<<reactor<<", parent(Component): "<<parent<<", id:"<<id;
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
        state(State::Open);
    }

    void do_open() { }
    
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


template<class Self, class ReactorT=io::Reactor, typename StateT=core::State, typename...ArgsT>
using BasicApp = io::BasicService<Self, ReactorT, StateT, ArgsT...>;


/// Adds PeersMap
template<class Self, class PeerT, typename StateT=core::State, typename...>
class BasicPeerService : public BasicService<Self, typename PeerT::Reactor, StateT>
{
    FT_SELF(Self);
    using Base = BasicService<Self, typename PeerT::Reactor, StateT>;
  public:
    using Peer = PeerT;
    using PeerPtr = std::unique_ptr<Peer>;
    using Endpoint = typename PeerT::Endpoint;
    using Packet = typename Peer::Packet;
    using PeersMap = io::PeersMap<Peer, PeerPtr>;
    using typename Base::Reactor;
    using Socket = typename Peer::Socket;
    using Transport = typename Peer::Transport;
    using typename Base::Ref;
  public:
    using typename Base::State;
    using Base::open, Base::close;
    
    using Base::Base;
    BasicPeerService(const Endpoint&ep, Reactor* reactor=nullptr, Component* parent=nullptr, Identifier id={})
    : local_(ep)
    , Base(reactor, parent, id) {}

    void do_open() {
        TOOLBOX_INFO << "opening "<< peers_.size() << " peers";
        for_each_peer([this](auto& peer) { 
            self()->do_open(peer);
        });
    }
    
    Endpoint& local() { return local_; }
    const Endpoint& local() const { return local_; }
    void local(const Endpoint& ep) { local_ = ep; }

    void do_open(Peer& peer) {
        peer.open();
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
        close();
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
    auto peers() const { return std::cref(peers_); }
    auto peers() { return std::ref(peers_); }  

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
        write_.set_slot(done);
        write_.pending(0);
        for_each_peer([&](auto& peer) {
             if(self()->route(peer, m.topic(), m.instrument_id())) {
              write_.inc_pending();
              self()->async_write(peer, m, write_.get_slot());
            }
        });
    }
    /// route everythere by default
    bool route(Peer& peer, StreamTopic topic, InstrumentId instrument) {
        return true;
    }

  protected:
    PeersMap peers_;
    Endpoint local_; /// interface to bind to
    tb::PendingSlot<ssize_t, std::error_code> write_; // when write is done
};


template<class Self, class L, typename...O>
class BasicMultiService : public io::BasicService<Self, O...> {
    using Base = io::BasicService<Self, O...>;
    FT_SELF(Self);
public:
    //using Base::Base;
    using ServicesTuple = mp::mp_rename<mp::mp_transform<std::unique_ptr,L>, std::tuple>; /// tuple
    using Base::parent;
    
    explicit BasicMultiService(Reactor* reactor=nullptr, Component* parent=nullptr, Identifier id={})
    : Base(reactor, parent, id) {
        TOOLBOX_DUMPV(5)<<"Service::ctor, self:"<<self()<<", reactor:"<<reactor<<", parent(Component): "<<parent<<", id:"<<id;
        //mp::tuple_for_each(services_,[&](auto& svc){
        //    using ServiceT = std::decay_t<decltype(*svc)>;
        //    svc = self()->template make_service<ServiceT>();
        //});
    }
    template<typename Fn>
    int for_each_service(Fn&& fn) {
        int count = 0;
        mp::tuple_for_each(services_, [&](auto& ptr) {
            if(ptr!=nullptr) {
                fn(*ptr);
                ++count;
            }
        });
        return count;
    }

    template<class ServiceT>
    std::unique_ptr<ServiceT> make_service(StreamTopic topic) {
        auto svc =  std::unique_ptr<ServiceT>(new ServiceT());
        svc->parent(self());
        svc->topic(topic);
        self()->on_child_service_created(*svc);
        return svc;
    }

    template<class ServiceT>
    void on_child_service_created(ServiceT& svc) {}

    template<class R>
    struct is_compatible_sink {
        template<typename S>
        struct fn: std::is_same<typename S::element_type::Type, R> {};
    };

    template<class T>
    auto& service(core::StreamTopic topic) {
        static_assert(!std::is_pointer_v<T>);
        static_assert(mp::mp_count_if_q<ServicesTuple, is_compatible_sink<T>>::value==1, "L does not contain T");
        constexpr std::size_t index = mp::mp_find_if_q<ServicesTuple, is_compatible_sink<T>>::value;
        auto& ptr = std::get<index>(services_);
        if(ptr==nullptr) {
            // lazy
            using ServiceT = std::decay_t<decltype(*ptr)>;
            ptr = self()->template make_service<ServiceT>(topic);
        }
        return *ptr;
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
        for_each_service([](auto& svc) {
            svc.close();
        });
    }

    template<class T>
    void write(const T& val) {
       service<T>().write(val);
    }

    /// write
    template<class T, typename DoneT>
    void async_write(const T& val, DoneT done) {
       service<T>().async_write(val, std::forward<DoneT>(done));
    }
protected:
    ServicesTuple services_;
};

} // ft::io