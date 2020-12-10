#pragma once
#include <ft/utils/Common.hpp>
#include "ft/core/Instrument.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/Component.hpp"
#include "ft/core/EndpointStats.hpp"
#include "ft/core/Requests.hpp"
#include "ft/core/Tick.hpp"
#include "ft/io/Connection.hpp"
#include "toolbox/net/Endpoint.hpp"
#include "ft/io/Service.hpp"

namespace ft::io {

namespace tb = toolbox;


template<typename ProtocolT, typename ConnT, typename ReactorT=tb::Reactor>
class BasicMdServer :  public io::BasicService<BasicMdServer<ProtocolT, ConnT, ReactorT>
                                              ,ProtocolT, ConnT, ReactorT>
{
public:
    using This = BasicMdServer<ProtocolT, ConnT, ReactorT>;
    using Base = io::BasicService<This, ProtocolT, ConnT, ReactorT>;
    using typename Base::Protocol;
    using typename Base::Connection;
    using typename Base::BinaryPacket;
    using typename Base::Socket;
    using typename Base::Endpoint;
    using typename Base::State;
    using typename Base::Connections;

    using Base::Base;

    using Base::state, Base::is_open, Base::url, Base::reactor;
    using Base::open, Base::close, Base::reopen, Base::connections;

    Connections prepare(const core::Parameters& params) {
        Connections conns;
        for(auto p: params) {
            std::string_view type = p["type"].get_string();
            auto proto = utils::str_suffix(type, '.');
            if(Connection::supports(proto)) {
                for(auto e : p["urls"]) {
                    std::string url = e.get_string().data();
                    auto& conn = conns.emplace_back(reactor());
                    conn.url(url);
                    conn.received().connect(tb::bind<&Protocol::on_packet>(&protocol_));
                }
            }
        }
        return conns;
    }

    void on_parameters_updated(const core::Parameters& params) {
        TOOLBOX_INFO<<"MdServer::on_parameters_updated"<<params;
        auto conns_p = params["connections"];
        protocol_.on_parameters_updated(conns_p);
        auto conns = prepare(conns_p);
        reopen(conns);
    }

    core::SubscriptionSignal& subscriptions(core::StreamName id) { return subscriptions_; }

    core::TickSink ticks(core::StreamName id) {
      return tb::bind<&This::write_tick>(this);
    }

    core::InstrumentSink instruments(core::StreamName id) {
      return tb::bind<&This::write_instrument>(this);
    }

    void write_tick(const core::Tick& tick) {
      for(auto &conn: connections()) {
        conn.async_write(tb::ConstBuffer{&tick, tick.length()}, tb::bind<&This::on_write>(this));
      }
      //conn_.async_write(ConstBuffer buffer, Slot<ssize_t, std::error_code> slot)
    }

    void on_write(ssize_t size, std::error_code ec) {

    }

    void write_instrument(const core::VenueInstrument& ins) {

    }

private:
    Connections connections_;
    Protocol protocol_;
    core::SubscriptionSignal subscriptions_;
};

}
