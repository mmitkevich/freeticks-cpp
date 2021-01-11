#pragma once

#include "ft/core/Component.hpp"
#include "ft/core/Parameters.hpp"
#include "toolbox/io/Reactor.hpp"
#include "toolbox/io/Runner.hpp"
#include "ft/io/Routing.hpp"
#include "toolbox/util/Slot.hpp"
#include "ft/io/Conn.hpp"
#include <exception>
#include <stdexcept>

namespace ft::io {

using Reactor = toolbox::io::Reactor;

Reactor* current_reactor();

/// Reactor-aware service
template<class ReactorT>
class Service : public core::Component {
    using This = Service<ReactorT>;
  public:
    using Reactor = ReactorT;
    //template<class SocketT>
    //using Connection = typename ft::io::Conn<SocketT, This>;
  public:
    explicit Service(Reactor* reactor=nullptr)
    : reactor_(reactor) {}

    Reactor* reactor() { assert(reactor_); return reactor_; }
    const Reactor* reactor() const { assert(reactor_); return reactor_; }
  protected:
    Reactor* reactor_{};
};

/// Reactor-aware service with state
template<class Self, class ReactorT, typename StateT, class ParentT>
class BasicService: public core::BasicComponent<Self, ParentT, Service<ReactorT>>
, public core::BasicParameterized<Self>
, public core::BasicStates<StateT>
{
    FT_MIXIN(Self);
    using Base = core::BasicComponent<Self, ParentT, Service<ReactorT>>;
    using States = core::BasicStates<StateT>;
  public:
    using Reactor = ReactorT;
    using States::state, Base::parent;
    using Base::reactor;

    explicit BasicService(Reactor* reactor=nullptr)
    : Base(reactor) {}

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

    void open() {}
    
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

    void close() {}


    /// @returns true if Service is open
    bool is_open() const { return state()==State::Started; }
    
    /// @returns true if Service is opening 
    bool is_opening() const { return state()==State::Starting; }
};

/// Adds PeersMap
template<class Self, class PeerT, typename StateT, class ParentT>
class BasicPeerService : public BasicService<Self, typename PeerT::Reactor, StateT, ParentT>
{
    using Base = BasicService<Self, typename PeerT::Reactor, StateT, ParentT>;
  public:
    using Peer = PeerT;
    using Endpoint = typename PeerT::Endpoint;
    using Packet = typename Peer::Packet;
    using PeersMap = io::PeersMap<Peer>;
    using typename Base::Reactor;
    using Socket = typename Peer::Socket;
    using Transport = typename Socket::Protocol;
  public:
    using Base::Base;

    /// open all peers
    void open() {
        TOOLBOX_INFO << "opening "<< peers().size() << " peers";
        for(auto& [ep, c]: peers_) {
            c.open();
        }
        Base::open();
    }
    
    /// close all peers
    void close() {  
        TOOLBOX_INFO << "closing "<< peers().size() << " peers";
        for(auto & [ep, c] : peers_) {
            c.close();
        }
        Base::close();  
    }

    /// clear all peers
    void clear() {
        peers_.clear();
    }

    /// close and remove peer identified by endpoint
    /// @returns true if peer was found, false overwise
    bool shutdown(const Endpoint& ep) {
        auto it = peers_.find(ep);
        if(it!=peers_.end()) {
            it->second.close();
            peers_.erase(it);
            return true;
        }
        return false;
    }

    /// @returns peers by endpoint
    const PeersMap& peers() const { return peers_; }

    /// @brief adds/replaces peer
    /// @returns pointer to peer added
    Peer* emplace(Peer&& val) {
        auto it = peers_.emplace(val.remote(), std::move(val));
        auto& peer =  it.first->second;
        peer.parent(this);
        return &peer;
    }

    template<typename Fn>
    int for_each_peer(const Fn& fn) {
      for(auto& [ep, peer]: peers_) {
        fn(peer);
      }
      return peers_.size();
    }
  protected:
    PeersMap& peers() { return peers_; }  
    PeersMap peers_;
};

} // ft::io