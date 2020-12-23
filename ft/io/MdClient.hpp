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


/// Stateless MD client, uses multiple connections of same protocol
template<typename DerivedT, typename ProtocolT, typename ConnT>
class BasicMdClient : public io::BasicClient< DerivedT, ConnT>
{
    using This = BasicMdClient<DerivedT, ProtocolT, ConnT>;
    using Base = io::BasicClient<DerivedT, ConnT>;
    friend Base;
    using Self = DerivedT;
    using Base::self;
public:
    using Protocol = ProtocolT;
    using typename Base::Connection;
    using typename Base::Connections;
    using typename Base::BinaryPacket;
    using typename Base::Socket;
    using typename Base::Endpoint;
    using typename Base::State;
public:
    using Base::Base;
    using Base::state, Base::url, Base::reactor;
    using Base::is_open, Base::reopen;
    using Base::make_connections, Base::connections;
    using Base::async_read, Base::async_write;
    using Base::async_connect;

    Protocol& protocol() {   return protocol_; }

    void open() {
        Base::open();
        protocol().open();
    }
    
    void close() {
        Base::close();
        protocol().close();
    }
    
    auto& stats() {
        return protocol().stats();
    }

    using Base::async_request;
    using tt = MdClientTraits::async_subscribe_t<Base>;

    // crtp override
    template<typename RequestT, typename ResponseT>
    void async_request(Connection& conn, const RequestT& req, 
        tb::Slot<const ResponseT&, std::error_code> slot, tb::DoneSlot done) 
    {
        // use protocol to serialize
        protocol().async_write(conn, req, done);
    }

    /// connectable streams
    core::TickStream& ticks(core::StreamTopic topic) {
        return protocol().ticks(topic);
    }

    core::InstrumentStream& instruments(core::StreamTopic topic) {
        return protocol().instruments(topic);
    }
public:
    void on_parameters_updated(const core::Parameters& params) {
       // TOOLBOX_DUMP_THIS;
        TOOLBOX_INFO << "MdClient::on_parameters_updated"<<params;
        auto conns_par = params["connections"];
        protocol().on_parameters_updated(conns_par);
        auto conns = make_connections(conns_par);
        reopen(conns);
    }
protected:
    /// forward to protocol
    void on_packet(const BinaryPacket& packet, tb::DoneSlot done) {
        protocol().on_packet(packet, done);     // will do all the stuff, then call done()
    }

    void on_idle() {
        std::stringstream ss;
        protocol_.stats().report(ss);
        TOOLBOX_INFO << ss.str();
    }
protected:
    Protocol protocol_;
};

template<typename ProtocolT, typename ConnT>
class MdClient : public BasicMdClient<MdClient<ProtocolT, ConnT>, ProtocolT, ConnT>
{
    using This = MdClient<ProtocolT, ConnT>;
    using Base = BasicMdClient<This, ProtocolT, ConnT>;
public:
    using Base::Base;
};

} // ft::mcasst