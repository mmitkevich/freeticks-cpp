#pragma once
#include "ft/utils/Common.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/Component.hpp"

#include "toolbox/net/EndpointFilter.hpp"

#include "toolbox/sys/Time.hpp"
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
template<typename ProtocolT, typename ConnT>
class BasicMdClient : public io::BasicClient< BasicMdClient<ProtocolT, ConnT>, ProtocolT, ConnT>
{
    using This = BasicMdClient<ProtocolT, ConnT>;
    using Base = io::BasicClient<This, ProtocolT, ConnT>;
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
    using Base::state, Base::url, Base::reactor, Base::protocol;
    using Base::is_open, Base::open, Base::close, Base::reopen;
    using Base::make_connections;
    
    void on_parameters_updated(const core::Parameters& params) {
       // TOOLBOX_DUMP_THIS;
        TOOLBOX_INFO<<"MdClient::on_parameters_updated"<<params;
        auto conns_p = params["connections"];
        protocol().on_parameters_updated(conns_p);
        auto conns = make_connections(conns_p);
        reopen(conns);
    }

    core::TickStream& ticks(core::StreamName stream) {
        return protocol().ticks(stream);
    }
    core::InstrumentStream& instruments(core::StreamName stream) {
        return protocol().instruments(stream);
    }
};

} // ft::mcasst