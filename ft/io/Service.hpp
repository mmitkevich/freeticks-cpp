#pragma once

#include "ft/core/Component.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/State.hpp"
#include "toolbox/io/Reactor.hpp"
#include "toolbox/io/Runner.hpp"
#include "ft/io/Routing.hpp"
#include "toolbox/util/Slot.hpp"
#include <exception>
#include <stdexcept>

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

    BasicReactiveComponent(Reactor* reactor=nullptr, core::Component* parent=nullptr)
    : Base(parent)
    , Reactive(reactor) {}
    
    Self* parent() { assert(parent_); return static_cast<Self*>(parent_);}
    const Self* parent() const { assert(parent_); return static_cast<const Self*>(parent_); }
    void parent(Self* val) { assert(val);  parent_ = val;}
};

/// Reactor-aware service with state
template<class Self, class ReactorT=io::Reactor, typename StateT=core::State, typename...>
class BasicService: public io::BasicReactiveComponent<ReactorT>
, public core::BasicParameterized<Self>
, public core::BasicStateful<StateT>
{
    FT_MIXIN(Self);
    using Base = io::BasicReactiveComponent<ReactorT>;
    using Stateful = core::BasicStateful<StateT>;
  public:
    using Reactor = ReactorT;
    using Stateful::state, Base::parent;
    using Base::reactor;
    
    explicit BasicService(Reactor* reactor=nullptr, Component* parent=nullptr)
    : Base(reactor, parent) {}

    // start component, throws only std::runtime_error
    void start() {
        try {
            self()->open();
        } catch(std::exception& e) {
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
template<class Self, class PeerT, typename StateT=core::State, typename...Ts>
class BasicPeerService : public BasicService<Self, typename PeerT::Reactor, StateT, Ts...>
{
    FT_MIXIN(Self);
    using Base = BasicService<Self, typename PeerT::Reactor, StateT, Ts...>;
  public:
    using Peer = PeerT;
    using Endpoint = typename PeerT::Endpoint;
    using Packet = typename Peer::Packet;
    using PeersMap = io::PeersMap<Peer>;
    using typename Base::Reactor;
    using Socket = typename Peer::Socket;
    using Transport = typename Peer::Transport;
  public:
    using Base::Base;
    using Base::State;
    using Base::open, Base::close;
    
    void do_open() {
        TOOLBOX_INFO << "opening "<< peers().size() << " peers";
        for_each_peer([this](auto& peer) { 
            self()->do_open(peer);
        });
    }

    void do_open(Peer& peer) {
        peer.open();
    }

    void do_close() {
        TOOLBOX_INFO << "closing "<< peers().size() << " peers";
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
    auto make_peer(ArgsT... args) { return std::unique_ptr<Peer>(new Peer(std::forward<ArgsT>(args)...)); };

    Peer* get_peer(const Endpoint& ep) {
        auto it = peers_.find(ep);
        if(it!=peers_.end()) {
            return it->second;
        }
        return nullptr;
    }
    /// close and remove peer identified by endpoint
    /// @returns true if peer was found, false overwise
    bool shutdown(const Endpoint& ep) {
        auto it = peers_.find(ep);
        if(it!=peers_.end()) {
            it->second->close();
            peers_.erase(it);
            return true;
        }
        return false;
    }

    /// @returns peers by endpoint
    const PeersMap& peers() const { return peers_; }


    template<typename Fn>
    int for_each_peer(const Fn& fn) {
      for(auto& [ep, peer]: peers_) {
        fn(*peer);
      }
      return peers_.size();
    }
    Peer& emplace_peer(std::unique_ptr<Peer> peer) {
        auto* ptr = peer.get();
        peers_.emplace(peer->remote(), std::move(peer));
        return *ptr;
    }
  protected:
    PeersMap& peers() { return peers_; }  
    PeersMap peers_;
};

} // ft::io