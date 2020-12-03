#pragma once
#include "ft/utils/Common.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/Component.hpp"

#include "toolbox/net/EndpointFilter.hpp"

#include "toolbox/sys/Time.hpp"
#include "toolbox/util/String.hpp"
#include "ft/core/MdClient.hpp"
#include "ft/core/EndpointStats.hpp"

#include "ft/io/Connections.hpp"

#include <string_view>
#include <system_error>
#include <vector>

namespace ft::io {

/// MD client, uses multiple connections of same protocol
template<typename ProtocolT>
class MdClient : public core::BasicComponent<MdClient<ProtocolT>> {
public:
    using This = MdClient<ProtocolT>;
    using Base = core::BasicComponent<This>;
    using Protocol = ProtocolT;
    using IpProtocol = tb::UdpProtocol;
    using Stats = core::EndpointStats<tb::BasicIpEndpoint<IpProtocol>>;
    static constexpr IpProtocol IpProto = IpProtocol::v4();
    using Connection = io::McastConn;
    using BinaryPacket = typename Connection::Packet;
    using Socket = typename Connection::Socket;
    using Endpoint = typename Connection::Endpoint;
    using State = typename Base::State;

    using Connections = std::vector<Connection>;
public:
    template<typename...ArgsT>
    MdClient(tb::Reactor* reactor, ArgsT...args)
    : Base(reactor)
    , protocol_(std::forward<ArgsT>(args)...)
    {}
    Protocol& protocol() {   return protocol_; }
    Connections& connections() { return connections_; }
    
    auto& stats() { return protocol_.stats(); }
    auto& gw_stats() { return stats_; }
    
    using Base::parameters;
    using Base::state;
    using Base::url;
    using Base::reactor;

    Connections make_connections(const core::Parameters& params) {
        Connections conns;
        for(auto p: params) {
            std::string_view type = p["type"].get_string();
            using namespace std::literals::string_view_literals;
            if(tb::ends_with(type, ".mcast"sv)) {
                for(auto e : p["urls"]) {
                    std::string url = e.get_string().data();
                    Endpoint ep =  tb::parse_ip_endpoint<IpProtocol>(url, 0, AF_INET);
                    conns.push_back(Connection {reactor()});
                    auto& conn = conns.back();
                    conn.endpoint(ep, if_name_);
                    conn.received().connect(tb::bind<&This::on_packet>(this));
                }
            }
        }
        return conns;
    }

    // dispatch parameters
    void on_parameters_updated(const core::Parameters& params) {
        TOOLBOX_INFO<<"McastMdClient::on_parameters_updated"<<params;
        if_name_ = params.value_or("interface", std::string{});
        auto conns_p = params["connections"];
        protocol_.on_parameters_updated(conns_p);
        auto conns = make_connections(conns_p);
        // TODO: track individual changes, disconnect/reconnect only changed 
        bool was_opened = is_open();
        if(was_opened)
            close();
        std::swap(connections_, conns);
        if(was_opened)
            open();
    }
    
    bool is_open() const {
        return state() != State::Stopped;
    }

    void close() {
        TOOLBOX_INFO << "Disconnecting "<< connections_.size() << " endpoints on interface '"<<if_name_<<"'";
        for(auto &c : connections_) {
            c.close();
        }
    }

    void open() {
        TOOLBOX_INFO << "Connecting "<< connections_.size() << " endpoints on interface '"<<if_name_<<"'";
        for(auto& c : connections_) {
            c.open();
        }     
    }


    void start() {
        state(State::Starting);
        using namespace std::literals::chrono_literals;
        idle_timer_ = reactor().timer(tb::MonoClock::now(), 10s, tb::Priority::Low, tb::bind<&This::on_idle_timer>(this));
        open();
        // FIXME: on_connected ?
        state(State::Started);
        protocol_.on_started();
    }

    void run() {

    }

    void stop() {
        state(State::Stopping);
        close();
        idle_timer_.cancel();
        state(State::Stopped);
    }
   
    void report(std::ostream& os) {
        protocol_.stats().report(os);
        stats_.report(os);
    }
    void on_idle_timer(tb::CyclTime now, tb::Timer& timer) {
        on_idle();
    }
    void on_idle() {
        report(std::cerr);
    }
    core::TickStream& ticks(core::StreamName stream) {
        return protocol_.ticks(stream);
    }
    core::VenueInstrumentStream& instruments(core::StreamName stream) {
        return protocol_.instruments(stream);
    }

    void on_packet(const BinaryPacket& pkt) {
        stats_.on_received(pkt);
        protocol_.on_packet(pkt);
    }

private:
    Connections connections_;
    std::string if_name_;
    Protocol protocol_;
    Stats stats_;
    toolbox::EndpointsFilter filter_;
    tb::Timer idle_timer_; 
};

} // ft::mcasst