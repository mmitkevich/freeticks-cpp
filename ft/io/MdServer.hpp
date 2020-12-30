
#pragma once
#include <ft/utils/Common.hpp>
#include <unordered_map>
#include "ft/core/Identifiable.hpp"
#include "ft/core/Instrument.hpp"
#include "ft/core/Subscriber.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/Component.hpp"
#include "ft/core/EndpointStats.hpp"
#include "ft/core/Requests.hpp"
#include "ft/core/Tick.hpp"
#include "ft/io/Conn.hpp"
#include "toolbox/net/Endpoint.hpp"
#include "ft/core/MdClient.hpp"
#include "ft/io/Server.hpp"
#include "ft/io/PeerConn.hpp"
#include "toolbox/util/Slot.hpp"
namespace ft::io {

/// Server handles multiple clients, each client is separate Connection
template<typename ProtocolT, typename PeerT, typename ServerSocketT>
class BasicMdServer :  public io::BasicServer<BasicMdServer<ProtocolT, PeerT, ServerSocketT>, ProtocolT, PeerT, ServerSocketT>
{
public:
    using This = BasicMdServer<ProtocolT, PeerT, ServerSocketT>;
    using Base = io::BasicServer<This, ProtocolT, PeerT, ServerSocketT>;
    using ServerSocket = ServerSocketT;
    using typename Base::Peer;
    using typename Base::PeersMap;
    using typename Base::Endpoint;
    using typename Base::Reactor;
  public:
    using Base::Base;

    using Base::state, Base::url, Base::reactor, Base::peers, Base::async_write;
    using Base::async_publish;

    core::SubscriptionSignal& subscribe(core::StreamTopic topic) { return subscribe_; }

    /// @example: server.ticks(StreamTopic::BestPrice)(Tick(200.1));
    core::TickSink& ticks(core::StreamTopic topic) { return ticks_; }

    core::InstrumentSink& instruments(core::StreamTopic topic) { return instruments_; }

private:
    core::SubscriptionSignal subscribe_;
    core::TickSink ticks_ = tb::bind< &Base::template async_publish<core::Tick> >(this);
    core::InstrumentSink instruments_ = tb::bind< &Base::template async_publish<core::InstrumentUpdate> >(this);
};

}
