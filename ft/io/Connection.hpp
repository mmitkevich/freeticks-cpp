#pragma once

#include "ft/core/Component.hpp"
#include "ft/core/StreamStats.hpp"
#include "toolbox/io/Buffer.hpp"
#include "toolbox/io/PollHandle.hpp"
#include "toolbox/net/IpAddr.hpp"
#include "toolbox/net/ParsedUrl.hpp"
#include "toolbox/net/Protocol.hpp"
#include "toolbox/net/Endpoint.hpp"
#include "toolbox/net/Packet.hpp"
#include "toolbox/io/Reactor.hpp"
#include "toolbox/io/Socket.hpp"
#include "toolbox/io/McastSocket.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/EndpointStats.hpp"
#include "ft/core/Stream.hpp"
#include "toolbox/net/ParsedUrl.hpp"
namespace ft::io {

namespace tb = toolbox;

class IConnection {
public:

};

template<typename ProtocolT>
std::string_view protocol_name(const ProtocolT& protocol) { return {}; }

inline std::string_view protocol_schema(tb::UdpProtocol& protocol) { return "udp"; }
inline std::string_view protocol_schema(tb::McastProtocol& protocol) { return "mcast"; }


template<typename SocketT, typename ReactorT=tb::Reactor>
class BasicConn : public core::BasicComponent<core::StreamState>, public SocketT
{
    using Base = SocketT;
    using This = BasicConn<SocketT>;
public:
    using Socket = SocketT;
    using typename Base::Endpoint;
    using typename Base::Protocol; 
    using PacketHeader = tb::PacketHeader<Endpoint>;
    using Packet = tb::Packet<PacketHeader, tb::ConstBuffer>;
    using Stats = core::EndpointStats<tb::BasicIpEndpoint<Protocol>>;
    using Reactor = ReactorT;
public:
    BasicConn(Reactor& reactor)
    : reactor_(reactor)
    {}

    using Base::read, Base::write, Base::async_recv;
    using Base::state, Base::open, Base::close;

    Packet& packet() { return packet_; } 
    const Packet& packet() const { return packet_; } 
    const Endpoint& remote() const { return remote_; } // "other side"
    const Endpoint& local() const  { return local_; }


    void remote(const Endpoint& ep) {
        remote_ = ep;
    }
    void local(const Endpoint& ep) {
        local_ = ep;
    }
    void url(const std::string& url) {
        url_ = tb::ParsedUrl {url};
        Endpoint ep =  tb::TypeTraits<Endpoint>::from_string(url_.url());
                        //tb::parse_dgram_endpoint(url, 0, AF_INET);
        remote(ep);
    }

    bool is_open() const { return state() == tb::SocketState::Connected; }

    tb::Signal<const Packet&>& received() { return received_; }
    
    constexpr std::size_t buffer_size() { return 4096; };
    
    void open() {
        Base::open(reactor_, remote().protocol());
        Base::bind(local());
        if constexpr (tb::SocketTraits::has_connect<Base>) {
            Base::async_connect(remote(), tb::bind<&This::on_connect>(this));
        } else {
            on_connect({});
        }
    }

    void on_connect(std::error_code ec) {
        Base::async_recv(buffer_.prepare(4096), 0, tb::bind<&This::on_recv>(this));
    }

    Stats& stats() { return stats_; }

    /// Slots {
    void on_recv(ssize_t size, std::error_code ec) {
        packet_.header().recv_timestamp(tb::CyclTime::now().wall_time());
        buffer_.commit(size);
        packet_.buffer() = {buffer_.buffer().data(), (std::size_t)size};
        received_(packet_);
        buffer_.consume(size);
        async_recv(buffer_.prepare(buffer_size()), 0, tb::bind<&This::on_recv>(this));
    }
    /// } Slots
protected:
    tb::Reactor& reactor_;
    Endpoint remote_;
    Endpoint local_;
    Packet packet_;
    tb::Buffer buffer_;
    tb::Signal<const Packet&> received_;
    std::string if_name_;
    Stats stats_;
    tb::ParsedUrl url_;
};

class DgramConn: public BasicConn<tb::DgramSocket> {
    using Base = BasicConn<tb::DgramSocket>;
public:
    using Endpoint = typename Base::Endpoint;
    using Packet = typename Base::Packet;
    using Socket = typename Base::Socket;
public:
    using Base::Base;
    using Base::open;
    using Base::close;
    using Base::received;
    using Base::state;

    static bool supports(std::string_view proto) { 
        return std::string_view{"udp"} == proto; 
    }
};

class McastConn: public BasicConn<tb::McastSocket> {
    using Base = BasicConn<tb::McastSocket>;
public:
    using Endpoint = typename Base::Endpoint;
    using Packet = typename Base::Packet;
    using Socket = typename Base::Socket;
public:
    using Base::Base;
    McastConn(McastConn&&)  = default;

    using Base::received;
    using Base::state;
    using Base::packet;
    using Base::local;
    
    using Base::on_recv;

    static bool supports(std::string_view proto) {
        return std::string_view{"mcast"} == proto;
    }

    void url(const std::string& url) {
        Endpoint ep =  tb::TypeTraits<Endpoint>::from_string(url);
        remote(ep);
        local(ep);
    }

    void remote(Endpoint ep) {
        ep.interface(tb::os::if_addrtoname(ep.interface()));
        Base::remote(ep);
        packet().header().dst(ep);  // for filter
    }

    Endpoint remote() const {
        return Base::remote();
    }

    void close() {
        Socket::disconnect(remote());
        /*
        if(!if_name().empty()) {
            Socket::leave_group(endpoint().address(), if_name().data());
            TOOLBOX_INFO << "leave_group, fd="<<get()<<", ep:'"<<endpoint()<<"', if:'"<<if_name()<<"'";
        } else {
            Socket::leave_group(endpoint().address(), 0u);
            TOOLBOX_INFO << "leave_group, fd="<<get()<<", ep:'"<<endpoint()<<"', if:0";
        }*/
        Base::close();
    }
};

}