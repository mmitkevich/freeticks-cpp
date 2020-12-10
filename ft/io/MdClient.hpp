#pragma once
#include "ft/utils/Common.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/Component.hpp"

#include "toolbox/net/EndpointFilter.hpp"

#include "toolbox/sys/Time.hpp"
#include "toolbox/util/String.hpp"
#include "ft/core/MdClient.hpp"

#include "ft/io/Service.hpp"
#include "ft/io/Connection.hpp"

#include <string_view>
#include <system_error>
#include <vector>

namespace ft::io {

/// MD client, uses multiple connections of same protocol
template<typename ProtocolT, typename ConnT, typename ReactorT=tb::Reactor>
class BasicMdClient : public io::BasicService<BasicMdClient<ProtocolT, ConnT, ReactorT>,
                             ProtocolT, ConnT, ReactorT>
{
public:
    using This = BasicMdClient<ProtocolT, ConnT, ReactorT>;
    using Base = io::BasicService<This, ProtocolT, ConnT, ReactorT>;
    using typename Base::Protocol;
    using typename Base::Connection;
    using typename Base::Connections;
    using typename Base::BinaryPacket;
    using typename Base::Socket;
    using typename Base::Endpoint;
    using typename Base::State;
public:
    template<typename...ArgsT>
    BasicMdClient(tb::Reactor* reactor, ArgsT...args)
    : Base(reactor)
    , protocol_(std::forward<ArgsT>(args)...)
    {
    }

    
    using Base::parameters;
    using Base::report;
    using Base::state, Base::url, Base::reactor;
    using Base::is_open, Base::open, Base::close, Base::reopen;

    Connections make_connections(const core::Parameters& params) {
        Connections conns;
        for(auto p: params) {
            std::string_view type = p["type"].get_string();
            auto proto = utils::str_suffix(type, '.');
            auto iface = p.value_or("interface", std::string{});
            if(Connection::supports(proto)) {
                for(auto e : p["urls"]) {
                    std::string url = e.get_string().data();
                    if(!iface.empty())
                        url = url+"|interface="+iface;
                    auto& conn = conns.emplace_back(reactor());
                    conn.url(url);
                    conn.received().connect(tb::bind<&Protocol::on_packet>(&protocol_));
                }
            }
        }
        return conns;
    }

    // dispatch parameters
    void on_parameters_updated(const core::Parameters& params) {
       // TOOLBOX_DUMP_THIS;
        TOOLBOX_INFO<<"MdClient::on_parameters_updated"<<params;
        auto conns_p = params["connections"];
        protocol_.on_parameters_updated(conns_p);
        auto conns = make_connections(conns_p);
        reopen(conns);
    }

    core::TickStream& ticks(core::StreamName stream) {
        return protocol_.ticks(stream);
    }
    core::InstrumentStream& instruments(core::StreamName stream) {
        return protocol_.instruments(stream);
    }
private:
    std::string if_name_;
    Protocol protocol_;
    toolbox::EndpointsFilter filter_;
};

} // ft::mcasst