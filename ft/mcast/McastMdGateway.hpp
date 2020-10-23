#pragma once
#include "ft/core/Parameterized.hpp"
#include "ft/io/Reactor.hpp"
#include "ft/utils/Common.hpp"

#include "toolbox/io/Buffer.hpp"
#include "toolbox/io/Reactor.hpp"
#include "toolbox/net/Endpoint.hpp"
#include "toolbox/net/Protocol.hpp"
#include "toolbox/net/EndpointFilter.hpp"
#include "toolbox/net/McastSock.hpp"
#include "toolbox/sys/Time.hpp"
#include "ft/core/MdGateway.hpp"
#include "ft/core/EndpointStats.hpp"
#include <system_error>
#include <vector>

namespace tbi = toolbox::io;

namespace ft::mcast {

template<typename ProtocolT, typename SocketT>
class BasicMcastDgram {
public:
    using Endpoint = toolbox::BasicIpEndpoint<ProtocolT>;
    using Socket = SocketT;
    using This = BasicMcastDgram<ProtocolT, SocketT>;
public:
    BasicMcastDgram(toolbox::Reactor& reactor, Endpoint ep, std::string_view if_name)
    : reactor_(reactor)
    , endpoint_(ep)
    , if_name_(if_name)
    {}
    
    Endpoint dst() const { return endpoint_; }
    Endpoint src() const { return endpoint_; }
    tb::WallTime recv_timestamp() const { return recv_timestamp_; }

    char* data() { return (char*)buf_.data().data(); }
    const char* data() const { return (char*)buf_.data().data(); }
    std::size_t size() const { return buf_.size(); }

    void connect() {
        assert(socket.empty());
        std::error_code ec {};
        socket_ = Socket(endpoint_.protocol(), ec);
        if(ec)
            throw std::system_error{tb::make_sys_error(ec.value()), "mcast_socket"};
        socket_.bind(src(), ec);
        TOOLBOX_INFO << "bind '"<<endpoint_<<"', ec:"<<ec;
        if(ec)
            throw std::system_error{tb::make_sys_error(ec.value()), "mcast_bind"};
        if(!if_name_.empty()) {
            socket_.join_group(src().address(), if_name_.data(), ec);
            TOOLBOX_INFO << "join_group '"<<src()<<"', if:'"<<if_name_<<"', ec:"<<ec;                
        } else {
            socket_.join_group(src().address(), 0u, ec);
            TOOLBOX_INFO << "join_group '"<<src()<<"', if:0, ec:"<<ec;                
        }
        if(ec)
            throw std::system_error{tb::make_sys_error(ec.value()), "mcast_join_group"};            
        assert(!socket.empty());
        sub_recv_ = reactor_.subscribe(socket_.get(), tb::EpollIn, tb::bind<&This::on_recv>(this));
    }
    void disconnect() {
        assert(!socket.empty());
        std::error_code ec {};
            if(!if_name_.empty()) {
            socket_.leave_group(src().address(), if_name_.data(), ec);
            TOOLBOX_INFO << "leave_group '"<<src()<<"', if:'"<<if_name_<<"', ec:"<<ec;                
        } else {
            socket_.leave_group(src().address(), 0u, ec);
            TOOLBOX_INFO << "leave_group '"<<src()<<"', if:0, ec:"<<ec;                
        }
        socket_.close();
        socket_ = Socket();
        assert(socket_.empty());
        sub_recv_.reset();
    }
    void on_recv(tb::CyclTime now, int fd, unsigned events) {
        recv_timestamp_ = now.wall_time();
        std::size_t size = socket_.recv(buf_.prepare(4096), 0);
        TOOLBOX_INFO<<"recv "<<size<<" bytes from "<<src();
        packet_(*this);
    }
    bool is_connected() const { return !socket_.empty(); }
private:
    tb::Signal<const This&> packet_;
    Endpoint endpoint_;
    Socket socket_;
    std::string_view if_name_;
    toolbox::Buffer buf_;
    toolbox::Reactor& reactor_;
    tb::Reactor::Handle sub_recv_;
    tb::WallTime recv_timestamp_;
};

using McastDgram = BasicMcastDgram<tb::UdpProtocol, tb::McastSock>;

template<typename ProtocolT>
class McastMdGateway : public core::BasicMdGateway<McastMdGateway<ProtocolT>> {
public:
    using Base = core::BasicMdGateway<McastMdGateway<ProtocolT>>;
    using This = McastMdGateway<ProtocolT>;
    using Protocol = ProtocolT;
    using Stats = core::EndpointStats;
    using TransportProtocol = toolbox::UdpProtocol;
    static constexpr TransportProtocol transport_protocol = TransportProtocol::v4();
    using Connection = McastDgram;
    using Socket = typename Connection::Socket;
    using Endpoint = typename Connection::Endpoint;
    using State = typename Base::State;

    using Connections = std::vector<Connection>;
public:
    template<typename...ArgsT>
    McastMdGateway(tb::Reactor& reactor, ArgsT...args)
    : reactor_(reactor)
    , protocol_(std::forward<ArgsT>(args)...)
    {
        
    }

    Protocol& protocol() {   return protocol_; }
    Connections& connections() { return connections_; }
    
    Stats& stats() { return stats_; }
    
    using Base::parameters;
    using Base::state;

    Connections make_connections(const core::Parameters& params) {
        Connections conns;
        for(auto stream_p: params) {
            for(auto e : stream_p["urls"]) {
                std::string url = e.get_string().data();
                Endpoint ep =  tbn::parse_ip_endpoint<TransportProtocol>(url, 0, AF_INET);
                Connection c(reactor_, ep, if_name_);
                conns.push_back(std::move(c));
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
        TOOLBOX_INFO << "Disconnecting "<< connections_.size() << " connections";        
        for(auto &c : connections_) {
            c.disconnect();
        }
    }

    void connect() {
        TOOLBOX_INFO << "Connecting "<< connections_.size() << " endpoints";        
        for(auto& c : connections_) {
            c.connect();
        }     
    }

    void url(std::string_view url) {
        url_ = url;
    }
    
    std::string_view url() const { return url_; }

    void start() {
        connect();
    }

    void run() {

    }

    void stop() {
        disconnect();
    }
   
    void report(std::ostream& os) {
        protocol_.stats().report(os);
        stats_.report(os);
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

private:
    Connections connections_;
    std::string url_;
    std::string if_name_;
    Protocol protocol_;
    Stats stats_;
    toolbox::EndpointsFilter filter_;
    tbi::Reactor& reactor_;
    std::atomic<bool> stop_ {false};       
};

} // ft::mcasst