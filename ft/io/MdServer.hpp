
#pragma once
#include <ft/utils/Common.hpp>
#include <unordered_map>
#include "ft/core/Identifiable.hpp"
#include "ft/core/Instrument.hpp"
#include "ft/core/Subscription.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/Component.hpp"
#include "ft/core/EndpointStats.hpp"
#include "ft/core/Requests.hpp"
#include "ft/core/Tick.hpp"
#include "ft/io/Conn.hpp"
#include "ft/io/Routing.hpp"
#include "toolbox/net/Endpoint.hpp"
#include "ft/core/MdClient.hpp"
#include "ft/io/Server.hpp"
#include "toolbox/util/Slot.hpp"
namespace ft::io {


template<
  class Self
, class ProtocolT
, class PeerT
, class ServerSocketT
, class RoutingStrategyT
, typename StateT
> class BasicMdServer : public io::BasicServer<Self, ProtocolT, PeerT, ServerSocketT, RoutingStrategyT, StateT>
{
    FT_MIXIN(Self);
    using Base =  io::BasicServer<Self, ProtocolT, PeerT, ServerSocketT, RoutingStrategyT, StateT>;
public:
    using typename Base::Peer;
    using typename Base::Endpoint;
    using typename Base::Reactor;
  public:
    using Base::Base;

    using Base::state, Base::reactor;
    using Base::async_publish;

    core::SubscriptionSignal& subscribe(core::StreamTopic topic) { return subscribe_; }

    /// @example: server.ticks(StreamTopic::BestPrice)(Tick(200.1));
    core::TickSink& ticks(core::StreamTopic topic) { return ticks_; }

    core::InstrumentSink& instruments(core::StreamTopic topic) { return instruments_; }

private:
    core::SubscriptionSignal subscribe_;
    core::TickSink ticks_ = tb::bind< &Base::template async_publish<core::Tick> >(this);
    core::InstrumentSink instruments_ = tb::bind< &Base::template async_publish<core::InstrumentUpdate> >(this);
}; // BasicMdServer

template<
  class ProtocolT
, class PeerT
, class ServerSocketT
, class RoutingStrategyT = io::RoundRobinRoutingStrategy<PeerT>
, typename StateT = core::State
> class MdServer : public io::BasicMdServer<
  MdServer<ProtocolT, PeerT,  ServerSocketT, RoutingStrategyT, StateT>,
  ProtocolT, PeerT, ServerSocketT, RoutingStrategyT, StateT>
{
    using Base = io::BasicMdServer<
      MdServer<ProtocolT, PeerT, ServerSocketT, RoutingStrategyT, StateT>,
      ProtocolT, PeerT, ServerSocketT, RoutingStrategyT, StateT>;
  public:
    using Base::Base;
};

} // ft::io
