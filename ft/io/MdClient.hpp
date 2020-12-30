#pragma once
#include "ft/core/Requests.hpp"
#include "ft/utils/Common.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/Component.hpp"
#include "ft/core/MdClient.hpp"
#include "toolbox/net/EndpointFilter.hpp"

#include "toolbox/sys/Time.hpp"
#include "toolbox/util/Slot.hpp"
#include "toolbox/util/String.hpp"

#include "ft/io/Client.hpp"
#include "ft/io/Conn.hpp"

#include "ft/core/Tick.hpp"
#include "ft/core/Instrument.hpp"

#include <string_view>
#include <system_error>
#include <vector>

namespace ft::io {

template<typename ProtocolT, typename PeerT, typename ResponseTL=mp::mp_list<core::SubscriptionResponse> >
class BasicMdClient : public io::BasicClient< BasicMdClient<ProtocolT, PeerT, ResponseTL>, ProtocolT, PeerT, ResponseTL>
{
    using This = BasicMdClient<ProtocolT, PeerT, ResponseTL>;
    using Base = io::BasicClient<This, ProtocolT, PeerT, ResponseTL>;
    friend Base;
public:
    using typename Base::Protocol;
    using typename Base::Peer;
    using typename Base::PeersMap;
    using typename Base::Packet;
    using typename Base::Socket;
    using typename Base::Endpoint;
    using typename Base::State;
public:
    using Base::Base;
    using Base::state, Base::url, Base::reactor, Base::protocol;
    using Base::peers;
    using Base::async_read, Base::async_write;
    using Base::async_connect, Base::async_request;

    auto& stats() { return protocol().stats(); }

    /// connectable streams
    core::TickStream& ticks(core::StreamTopic topic) { return protocol().ticks(topic); }

    core::InstrumentStream& instruments(core::StreamTopic topic) { return protocol().instruments(topic); }
public:
    void on_parameters_updated(const core::Parameters& params) {
        Base::on_parameters_updated(params);
        protocol().on_parameters_updated(params["connections"]);
    }
protected:
    /// forward to protocol
    void on_packet(const Packet& packet, tb::DoneSlot done) {
        protocol().on_packet(packet, done);     // will do all the stuff, then call done()
    }

    void on_idle() {
        std::stringstream ss;
        protocol().stats().report(ss);
        TOOLBOX_INFO << ss.str();
    }
};

} // ft::mcasst