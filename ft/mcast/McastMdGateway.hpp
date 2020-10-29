#pragma once
#include "ft/core/Parameters.hpp"
#include "ft/core/Executor.hpp"
#include "ft/utils/Common.hpp"

#include "toolbox/io/Buffer.hpp"
#include "toolbox/io/Reactor.hpp"
#include "toolbox/net/Endpoint.hpp"
#include "toolbox/net/Protocol.hpp"
#include "toolbox/net/EndpointFilter.hpp"
#include "toolbox/net/McastSock.hpp"
#include "toolbox/sys/Time.hpp"
#include "toolbox/util/String.hpp"
#include "ft/core/MdGateway.hpp"
#include "ft/core/EndpointStats.hpp"
#include "toolbox/net/Packet.hpp"
#include <string_view>
#include <system_error>
#include <vector>

namespace tbi = toolbox::io;

namespace ft::mcast {

template<typename ProtocolT, typename SocketT>
class BasicMcastDgram {
public:
    using Endpoint = toolbox::BasicIpEndpoint<ProtocolT>;
    using Header = toolbox::net::Header<Endpoint>;
    using Socket = SocketT;
    using This = BasicMcastDgram<ProtocolT, SocketT>;
public:
    BasicMcastDgram(toolbox::Reactor& reactor, Endpoint ep, std::string_view if_name)
    : reactor_(reactor)
    , header_(ep, ep)
    , if_name_(if_name)
    {}
    
    const Header& header() const { return header_; }

    //char* data() { return (char*)buf_.str().data(); }
    const char* data() const { return (char*)buf_.str().data(); }
    std::size_t size() const { return buf_.size(); }
    std::string_view str() const { return buf_.str(); }

    const Endpoint& endpoint() const { return header_.src(); }
    void connect() {
        assert(socket_.empty());
        std::error_code ec {};
        socket_ = Socket(endpoint().protocol(), ec);
        if(ec)
            throw std::system_error{tb::make_sys_error(ec.value()), "mcast_socket"};
        socket_.bind(endpoint(), ec);
        TOOLBOX_INFO << "bind '"<<endpoint()<<"', ec:"<<ec;
        if(ec)
            throw std::system_error{tb::make_sys_error(ec.value()), "mcast_bind"};
        if(!if_name_.empty()) {
            socket_.join_group(endpoint().address(), if_name_.data(), ec);
            TOOLBOX_INFO << "join_group '"<<endpoint()<<"', if:'"<<if_name_<<"', ec:"<<ec;                
        } else {
            socket_.join_group(endpoint().address(), 0u, ec);
            TOOLBOX_INFO << "join_group '"<<endpoint()<<"', if:0, ec:"<<ec;                
        }
        if(ec)
            throw std::system_error{tb::make_sys_error(ec.value()), "mcast_join_group"};            
        assert(!socket_.empty());
        sub_recv_ = reactor_.subscribe(socket_.get(), tb::EpollIn, tb::bind<&This::on_recv>(this));
    }
    void disconnect() {
        assert(!socket_.empty());
        std::error_code ec {};
            if(!if_name_.empty()) {
            socket_.leave_group(endpoint().address(), if_name_.data(), ec);
            TOOLBOX_INFO << "leave_group '"<<endpoint()<<"', if:'"<<if_name_<<"', ec:"<<ec;                
        } else {
            socket_.leave_group(endpoint().address(), 0u, ec);
            TOOLBOX_INFO << "leave_group '"<<endpoint()<<"', if:0, ec:"<<ec;                
        }
        socket_.close();
        socket_ = Socket();
        assert(socket_.empty());
        sub_recv_.reset();
    }
    void on_recv(tb::CyclTime now, int fd, unsigned events) {
        header_.recv_timestamp(now.wall_time());
        std::size_t size = socket_.recv(buf_.prepare(4096), 0);
        buf_.commit(size);
        packet_(*this);
        buf_.consume(size);
    }
    bool is_connected() const { return !socket_.empty(); }
    tb::Signal<const This&> &packet() { return packet_; }
private:
    tb::Signal<const This&> packet_;
    Socket socket_;
    std::string_view if_name_;
    toolbox::Buffer buf_;
    toolbox::Reactor& reactor_;
    tb::Reactor::Handle sub_recv_;
    Header header_{};
};

using McastDgram = BasicMcastDgram<tb::UdpProtocol, tb::McastSock>;

template<typename ProtocolT>
class McastMdGateway : public core::BasicMdGateway<McastMdGateway<ProtocolT>> {
public:
    using Base = core::BasicMdGateway<McastMdGateway<ProtocolT>>;
    using This = McastMdGateway<ProtocolT>;
    using Protocol = ProtocolT;
    using TransportProtocol = toolbox::UdpProtocol;
    using Stats = core::EndpointStats<tb::BasicIpEndpoint<TransportProtocol>>;
    static constexpr TransportProtocol transport_protocol = TransportProtocol::v4();
    using BinaryPacket = McastDgram;
    using Connection = McastDgram;
    using Socket = typename Connection::Socket;
    using Endpoint = typename Connection::Endpoint;
    using State = typename Base::State;

    using Connections = std::vector<Connection>;
public:
    template<typename...ArgsT>
    McastMdGateway(tb::Reactor& reactor, ArgsT...args)
    : Base(&reactor)
    , protocol_(*this, std::forward<ArgsT>(args)...)
    {
        
    }
    using Base::reactor;

    Protocol& protocol() {   return protocol_; }
    Connections& connections() { return connections_; }
    
    Stats& stats() { return stats_; }
    
    using Base::parameters;
    using Base::state;

    Connections make_connections(const core::Parameters& params) {
        Connections conns;
        for(auto stream_p: params) {
            std::string_view type = stream_p["type"].get_string();
            using namespace std::literals::string_view_literals;
            if(tb::ends_with(type, ".mcast"sv))
            {
                for(auto e : stream_p["urls"]) {
                    std::string url = e.get_string().data();
                    Endpoint ep =  tbn::parse_ip_endpoint<TransportProtocol>(url, 0, AF_INET);
                    Connection c(reactor(), ep, if_name_);
                    conns.push_back(std::move(c));
                    conns.back().packet().connect(tb::bind<&This::on_packet>(this));
                }
            }
        }
        return conns;
    }

    // dispatch parameters
    void on_parameters_updated(const core::Parameters& params) {
        TOOLBOX_INFO<<"McastMdGateway::on_parameters_updated"<<params;
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

    void url(std::string_view url) {
        url_ = url;
    }
    
    std::string_view url() const { return url_; }

    void start() {
        state(State::Starting);
        using namespace std::literals::chrono_literals;
        idle_timer_ = reactor().timer(tb::MonoClock::now(), 10s, tb::Priority::Low, tb::bind<&This::on_idle_timer>(this));
        connect();
        // FIXME: on_connected ?
        state(State::Started);
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
    core::TickStream& ticks(core::StreamType streamtype) {
        return protocol_.ticks(streamtype);
    }
    core::VenueInstrumentStream& instruments(core::StreamType streamtype) {
        return protocol_.instruments(streamtype);
    }

    void on_packet(const BinaryPacket& pkt) {
        stats_.on_received(pkt);
        stats_.on_accepted(pkt);
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