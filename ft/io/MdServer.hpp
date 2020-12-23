
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
template<typename PeerT, typename ServerSocketT>
class BasicMdServer :  public io::BasicServer<BasicMdServer<PeerT, ServerSocketT>, PeerT, ServerSocketT>
{
public:
    using This = BasicMdServer<PeerT, ServerSocketT>;
    using Base = io::BasicServer<This, PeerT, ServerSocketT>;
    using ServerSocket = ServerSocketT;
    using typename Base::Peer;
    using typename Base::Peers;
    using typename Base::Endpoint;
    using typename Base::Reactor;
  public:
    using Base::Base;

    using Base::state, Base::url, Base::reactor, Base::peers;

    core::SubscriptionSignal& subscribe(core::StreamTopic topic) { return subscribe_; }

    core::TickSink& ticks(core::StreamTopic topic) {
      return ticks_;
    }

    core::InstrumentSink& instruments(core::StreamTopic topic) {
      return instruments_;
    }

    template<typename MessageT>
    void publish(const MessageT& message, tb::DoneSlot slot) {
      // decide who wants this tick and broadcast
      for(auto& [ep, peer]: peers()) {
        auto topic = message.topic();
        if(peer.subscription().test(topic)) {
          peer.async_write(message, slot);
        }
      }
    }

private:
    core::SubscriptionSignal subscribe_;
    core::TickSink ticks_ = tb::bind<&This::publish<core::Tick>>(this);
    core::InstrumentSink instruments_ = tb::bind<&This::publish<core::InstrumentUpdate>>(this);
};

}
