
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
#include "ft/io/Service.hpp"
#include "toolbox/io/Socket.hpp"
#include "toolbox/net/Endpoint.hpp"
#include "ft/core/Client.hpp"
#include "ft/io/Server.hpp"
#include "toolbox/util/Slot.hpp"
namespace ft::io {

template<
  class Self
, template<class...> class ProtocolM
, class PeerT
, class ServerSocketT
, typename StateT
> class BasicMdServer : public io::BasicServer<Self, PeerT, ServerSocketT, StateT>
, public ProtocolM<Self>
{
    FT_SELF(Self);
    using Base =  io::BasicServer<Self, PeerT, ServerSocketT, StateT>;
    using typename Base::PeersMap;
public:
    using typename Base::Peer;
    using typename Base::Endpoint;
    using typename Base::Reactor;
  public:
    using Base::Base;
    using Protocol = ProtocolM<Self>;
    using Base::state, Base::reactor;
    using Protocol::async_write;
    using Base::async_write;
    using Base::for_each_peer;

    template<typename T>
    using Slot = typename Protocol::template Slot<T>;

    void open() {
        Base::open();
        Protocol::open();
    }

    void close() {
        Protocol::close();
        Base::close();
    }

    void on_parameters_updated(const core::Parameters& params) {
        Base::on_parameters_updated(params);        
        Protocol::on_parameters_updated(params);
    }

    /// returns Stream::Slot
    core::Stream& slot(core::StreamTopic topic) { 
      switch(topic) {
        case core::StreamTopic::Instrument:
          return instruments_slot_; 
        case core::StreamTopic::BestPrice:
          return ticks_slot_;
        default:
          return Protocol::slot(topic);
      }
    }

    bool route(Peer& peer, StreamTopic topic, InstrumentId instrument) {
      bool result = peer.subscription().test(topic, instrument);
      return result;
    }

    core::SubscriptionSignal& subscription() { 
      return subscription_;
    }
protected:
    tb::PendingSlot<ssize_t, std::error_code> write_;
    // from client to server
    core::SubscriptionSignal subscription_;
    Slot<core::Tick> ticks_slot_{self()};
    Slot<core::InstrumentUpdate> instruments_slot_{self()};
}; // BasicMdServer

template<
  template<class> class ProtocolM
, class PeerT
, class ServerSocketT
, typename StateT = core::State
> class MdServer : public io::BasicMdServer<
  MdServer<ProtocolM, PeerT,  ServerSocketT, StateT>,
  ProtocolM, PeerT, ServerSocketT, StateT>
{
    using Base = io::BasicMdServer<
      MdServer<ProtocolM, PeerT, ServerSocketT, StateT>,
      ProtocolM, PeerT, ServerSocketT, StateT>;
  public:
    using Base::Base;
};

} // ft::io
