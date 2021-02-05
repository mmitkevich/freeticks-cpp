
#pragma once
#include <ft/utils/Common.hpp>
#include <stdexcept>
#include <unordered_map>
#include "ft/core/Identifiable.hpp"
#include "ft/core/Instrument.hpp"
#include "ft/core/Stream.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/Component.hpp"
#include "ft/core/EndpointStats.hpp"
#include "ft/core/Requests.hpp"
#include "ft/core/Tick.hpp"
#include "ft/io/Conn.hpp"
//#include "ft/io/Routing.hpp"
#include "toolbox/io/Socket.hpp"
#include "toolbox/net/Endpoint.hpp"
#include "ft/core/Client.hpp"
#include "ft/io/Server.hpp"
#include "toolbox/util/Slot.hpp"
namespace ft::io {


template<
  class Self
, class ProtocolT
, class PeerT
, class ServerSocketT
, typename StateT
> class BasicMdServer : public io::BasicServer<Self, ProtocolT, PeerT, ServerSocketT, StateT>
{
    FT_SELF(Self);
    using Base =  io::BasicServer<Self, ProtocolT, PeerT, ServerSocketT, StateT>;
    using typename Base::PeersMap;
public:
    using typename Base::Peer;
    using typename Base::Endpoint;
    using typename Base::Reactor;
  public:
    using Base::Base;
  
    using Base::state, Base::reactor, Base::protocol;
    using Base::async_write;
    using Base::for_each_peer;

    template<typename T>
    using Stream = typename Base::template Stream<T>;

    core::SubscriptionSignal& subscription(core::StreamTopic topic) { return protocol().subscription(); }

    core::Stream& stream(core::StreamTopic topic) { 
      switch(topic) {
        case core::StreamTopic::Instrument:
          return instruments_; 
        case core::StreamTopic::BestPrice:
          return ticks_;
        default:
          throw std::logic_error("no such stream");
      }
    }

    template<typename MessageT>
    void async_write(const MessageT& m, tb::SizeSlot done) {
        write_.set_slot(done);
        write_.pending(0);
        for_each_peer([&](auto& peer) {
            if(self()->check(peer, m)) {
              write_.inc_pending();
              self()->async_write(peer, m, write_.get_slot());
            }
        });
    }

    template<typename MessageT>
    bool check(Peer& peer, const MessageT& m) {
      StreamTopic topic = m.topic();
      auto& strm = stream(topic);
      bool result = strm.subscription().check(m);
      return result;
    }
protected:
    tb::PendingSlot<ssize_t, std::error_code> write_;
    //Router router_{self()};
    //core::SubscriptionSignal subscribe_;
    Stream<core::Tick> ticks_{self()};
    Stream<core::InstrumentUpdate> instruments_{self()};
}; // BasicMdServer

template<
  class ProtocolT
, class PeerT
, class ServerSocketT
, typename StateT = core::State
> class MdServer : public io::BasicMdServer<
  MdServer<ProtocolT, PeerT,  ServerSocketT, StateT>,
  ProtocolT, PeerT, ServerSocketT, StateT>
{
    using Base = io::BasicMdServer<
      MdServer<ProtocolT, PeerT, ServerSocketT, StateT>,
      ProtocolT, PeerT, ServerSocketT, StateT>;
  public:
    using Base::Base;
};

} // ft::io
