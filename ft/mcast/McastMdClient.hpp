#pragma once
#include "ft/utils/Common.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/Component.hpp"

#include "toolbox/net/EndpointFilter.hpp"

#include "toolbox/sys/Time.hpp"
#include "toolbox/util/String.hpp"
#include "ft/core/MdClient.hpp"
#include "ft/core/EndpointStats.hpp"

#include "ft/io/DgramConn.hpp"

#include <string_view>
#include <system_error>
#include <vector>

namespace ft::mcast {

template<typename ProtocolT>
class McastMdClient : public core::BasicComponent<McastMdClient<ProtocolT>, core::State> {
public:
    using This = McastMdClient<ProtocolT>;
    using Base = core::BasicComponent<This, core::State>;
    using Protocol = ProtocolT;
    using TransportProtocol = toolbox::UdpProtocol;
    using Stats = core::EndpointStats<tb::BasicIpEndpoint<TransportProtocol>>;
    static constexpr TransportProtocol transport_protocol = TransportProtocol::v4();
    using Connection = io::McastDgramConn;
    using BinaryPacket = typename Connection::Packet;
    using Socket = typename Connection::Socket;
    using Endpoint = typename Connection::Endpoint;
    using State = typename Base::State;

    using Connections = std::vector<Connection>;
public:
    template<typename...ArgsT>
    McastMdClient(tb::Reactor* reactor, ArgsT...args)
    : Base(reactor)
    , protocol_(std::forward<ArgsT>(args)...)
    {
        
    }
    using Base::reactor;
    Protocol& protocol() {   return protocol_; }
    Connections& connections() { return connections_; }
    
    auto& stats() { return protocol_.stats(); }
    auto& gw_stats() { return stats_; }
    
    using Base::parameters;
    using Base::state;
    //using Base::url;

    Connections make_connections(const core::Parameters& params) {
        Connections conns;
        for(auto stream_p: params) {
            std::string_view type = stream_p["type"].get_string();
            using namespace std::literals::string_view_literals;
            if(tb::ends_with(type, ".mcast"sv))
            {
                for(auto e : stream_p["urls"]) {
                    std::string url = e.get_string().data();
                    Endpoint ep =  tb::parse_ip_endpoint<TransportProtocol>(url, 0, AF_INET);
                    Connection c(reactor(), ep, if_name_);
                    conns.push_back(std::move(c));
                    conns.back().received().connect(tb::bind<&This::on_packet>(this));
                }
            }
        }
        return conns;
    }

    // dispatch parameters
    void on_parameters_updated(const core::Parameters& params) {
        TOOLBOX_INFO<<"McastMdClient::on_parameters_updated"<<params;
        if_name_ = params.value_or("interface", std::string{});
        auto streams_p = params["streams"];
        protocol_.on_parameters_updated(streams_p);
        auto conns = make_connections(streams_p);
        // TODO: track individual changes, disconnect/reconnect only changed 
        bool was_connected = is_connected();
        if(was_connected)
            disconnect();
        std::swap(connections_, conns);
        if(was_connected)
            connect();
    }
    
    bool is_connected() const {
        return state() != State::Stopped;
    }

    void disconnect() {
        TOOLBOX_INFO << "Disconnecting "<< connections_.size() << " endpoints on interface '"<<if_name_<<"'";
        for(auto &c : connections_) {
            c.disconnect();
        }
    }

    void connect() {
        TOOLBOX_INFO << "Connecting "<< connections_.size() << " endpoints on interface '"<<if_name_<<"'";
        for(auto& c : connections_) {
            c.connect();
        }     
    }


    void start() {
        state(State::Starting);
        using namespace std::literals::chrono_literals;
        idle_timer_ = reactor().timer(tb::MonoClock::now(), 10s, tb::Priority::Low, tb::bind<&This::on_idle_timer>(this));
        connect();
        // FIXME: on_connected ?
        state(State::Started);
        protocol_.on_started();
    }

    void run() {

    }

    void stop() {
        state(State::Stopping);
        disconnect();
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
    std::string url_;
    std::string if_name_;
    Protocol protocol_;
    Stats stats_;
    toolbox::EndpointsFilter filter_;
    tb::Timer idle_timer_; 
};

} // ft::mcasst