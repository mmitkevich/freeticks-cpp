
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
#include "ft/io/Service.hpp"
#include "toolbox/io/Socket.hpp"
#include "toolbox/net/Endpoint.hpp"
#include "ft/core/Client.hpp"
#include "ft/io/Server.hpp"
#include "toolbox/util/Slot.hpp"
namespace ft::io {

template<class Self
, template<class...> class ProtocolM
, class PeerT
, class ServerSocketT
, typename...O>
class BasicMdServer : public BasicServer<Self, PeerT, ServerSocketT, O...>
, public ProtocolM<Self>
{
    FT_SELF(Self);
    using Base = BasicServer<Self, PeerT, ServerSocketT, O...>;
public:
    using Base::Base;
    using typename Base::Peer;
    using Protocol = ProtocolM<Self>;
    
    using Base::open, Base::close; // resolve ambiguity with Protocol::open

    void do_open() {
        Base::do_open();
        Protocol::open();
    }

    void do_close() {
        Protocol::close();
        Base::do_close();
    }

    void on_parameters_updated(const core::Parameters& params) {
        Base::on_parameters_updated(params);        
        Protocol::on_parameters_updated(params);
    }

    void on_subscribe(Peer& peer, core::SubscriptionRequest& req) {
        Protocol::on_subscribe(peer, req); // notifies on subscription
        peer.subscription().set(req.topic(), req.instrument_id()); // modify peers' subscription
    }

    bool route(Peer& peer, StreamTopic topic, InstrumentId instrument) {
      bool result = peer.subscription().test(topic, instrument);
      return result;
    }

    template<class MessageT>
    void async_write_to(Peer& peer, const MessageT& m, tb::SizeSlot done) {
      Protocol::async_write_to(peer, m, done);
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

protected:
    template<class T>
    using Slot = typename Protocol::template Slot<T>;
    Slot<core::Tick> ticks_slot_{self()};
    Slot<core::InstrumentUpdate> instruments_slot_{self()};    
};


template<
  template<class> class ProtocolM
, class PeerT
, class ServerSocketT
> class MdServer : public io::BasicMdServer<MdServer<ProtocolM, PeerT, ServerSocketT>, ProtocolM, PeerT, ServerSocketT>
{
    using Base = io::BasicMdServer<MdServer<ProtocolM, PeerT,  ServerSocketT>,ProtocolM, PeerT, ServerSocketT>;
  public:
    using Base::Base;
};

} // ft::io
