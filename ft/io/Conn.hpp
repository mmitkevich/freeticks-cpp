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
#include "toolbox/io/DgramSocket.hpp"
#include "toolbox/io/McastSocket.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/EndpointStats.hpp"
#include "ft/core/Stream.hpp"
#include "toolbox/net/ParsedUrl.hpp"
#include "toolbox/net/Sock.hpp"
#include "toolbox/sys/Time.hpp"
#include "toolbox/util/Slot.hpp"
#include "ft/io/Heartbeats.hpp"
#include "ft/io/Service.hpp"

namespace ft::io {

class IConnection {
public:

};

template<typename ProtocolT>
std::string_view protocol_name(const ProtocolT& protocol) { return {}; }

constexpr std::string_view protocol_schema(tb::UdpProtocol& protocol) { return "udp"; }
constexpr std::string_view protocol_schema(tb::McastProtocol& protocol) { return "mcast"; }


/////
/// [web] WebSocket -- Connection --+              +--- Stream [BestPrice]
/// [udp] UdpSocket -- Connection --+-- Protocol --+
/// [tcp] TcpSocket -- Connection --+              +--- Stream [OrderBook]
///
///                 Client interconnects connections,  protocol and streams.
///    Client abstracts if many or single connection is used, does demultiplexing ,etc
/////
/// Socket is transport layer, either stream (has_connect) or dgram or mcast
/// Connection adds:
///  1) automatic reconnect
///  2) unified  async_connect/disconnect
///  3) buffered async_read/async_write 
/// Protocol is application-layer which could be shared between connections or individual per conn (use std::reference_wrapper)

template<typename DerivedT, typename SocketT, typename ServiceT>
class BasicConn : public io::BasicHeartbeats<DerivedT>
{
    using This = BasicConn<DerivedT, SocketT, ServiceT>;    
    using Base = core::BasicComponent<core::StreamState>;
    using Self = DerivedT;
    auto* self() { return static_cast<Self*>(this); }
    const auto* self() const { return static_cast<const Self*>(this); }
public:
    using Socket = SocketT;
    using Reactor = typename ServiceT::Reactor;
    using Endpoint = typename Socket::Endpoint;
    using Transport = typename Socket::Protocol;
    using Packet = tb::Packet<tb::ConstBuffer, Endpoint>;
    using Stats = core::EndpointStats<tb::BasicIpEndpoint<Transport>>;
    using Service = ServiceT;
public:
    BasicConn() = default;

    explicit BasicConn(std::string_view url) { 
        self()->url(url);
    }

    explicit BasicConn(Service* service, SocketT&& socket, Endpoint&& remote)
    :  service_(service)
    ,  socket_(std::move(socket))
    ,  remote_(std::move(remote)) {}

    BasicConn(const BasicConn&) = delete;
    BasicConn& operator=(const BasicConn&) = delete;

    BasicConn(BasicConn&&) = default;
    BasicConn& operator=(BasicConn&&) = default;
    
    ~BasicConn() {
        close();
    }

    /// attach to socket
    void open(Service* service, Transport transport) {
        assert(service);
        service_ = service;
        Reactor* r = service->reactor();
        socket_.open(*r, transport);
        if(local()!=Endpoint())
            socket_.bind(local());
    }

    void disconnect() {
        if constexpr(tb::SocketTraits::has_connect<SocketT>) {
            if(!socket().empty())
                socket().disconnect(remote()); // leave_group
        }
    }    
    
    void close() {
        disconnect();
        socket_.close(); // tcp disconnect
    }
    
    /// attached socket
    Socket& socket() { return socket_; }
    const Socket& socket() const { return socket_; }
    Service* service() { assert(service_); return service_; }
    Reactor* reactor() { return service()->reactor(); }

    /// connect  attached socket via async_connect if supported
    void async_connect(tb::DoneSlot slot) { 
        if constexpr (tb::SocketTraits::has_async_connect<SocketT>) {
            socket().async_connect(remote(), slot);
        } else if constexpr(tb::SocketTraits::has_connect<SocketT>) {
            socket().connect(remote());
            slot({});
        } else {
            slot({});
        }
    }
    
    static bool supports(std::string_view transport) { return transport == Transport::name; }

    bool is_open() const { return socket().state() == Socket::State::Open; }
    bool is_connecting() const { return socket().state() == Socket::State::Connecting; }

    /// local endpoint connections is bound to
    Endpoint& local() { return local_; }
    const Endpoint& local() const { return local_; }

    ///remote endpoint connection is connected to
    Endpoint& remote() { return remote_; }
    const Endpoint& remote() const { return remote_; }
    
    /// configure connection using text url
    void url(std::string_view url) {
        url_ = tb::ParsedUrl {url};
        remote_ =  tb::TypeTraits<Endpoint>::from_string(url_.url());
        auto iface = url_.param("interface");
        if(!iface.empty()) {
            local_ = tb::parse_ip_endpoint<Endpoint>(std::string_view(iface));
        }
    }

    /// connection stats
    Stats& stats() { return stats_; }

    bool can_write() const { return socket().can_write(); }
    bool can_read() const { return socket().can_read(); }

    /// adapt to stream or dgram socket
    void async_write(tb::ConstBuffer buf, tb::SizeSlot slot) {
        if constexpr(tb::SocketTraits::has_sendto<Socket>) {
            // udp
            socket().async_sendto(buf, remote(), slot);
        } else {
            // tcp
            socket().async_write(buf, slot);
        }
    }
    
    void async_read(tb::MutableBuffer buf, tb::SizeSlot slot) {
        packet_.buffer() = buf;
        if constexpr(tb::SocketTraits::has_recvfrom<Socket>) {
            // for mcast dst = mcast addr. FIXME: for unicast udp dst is local()
            packet_.header().dst() = remote();
            // will fill src
            socket().async_recvfrom(buf, packet_.header().src(), slot);
        } else {
            // stream packet received from tcp
            packet_.header().src() = remote();
            packet_.header().dst() = local();
            socket().async_read(buf, slot);
        }
    }
    Packet& packet() { return packet_; }
    Packet& packet(ssize_t size) { 
        packet_.buffer() = {packet_.buffer().data(), (std::size_t)size};
        return packet_;
    }
protected:
    Service* service_ {};
    Packet packet_;
    Socket socket_ {};
    Stats stats_;
    tb::ParsedUrl url_;
    tb::DoneSlot write_;
    Endpoint local_, remote_;
};

/// crtp multi bases for features like Heartbeated
template<typename SocketT>
class Conn : public BasicConn<Conn<SocketT>, SocketT, io::Service>
{
    using Base = BasicConn<Conn<SocketT>, SocketT,  io::Service>;
public:
    using Base::Base;
};



class BasicProtocol {
public:
    BasicProtocol() = default;

    BasicProtocol(const BasicProtocol&) = delete;
    BasicProtocol& operator=(const BasicProtocol&) = delete;

    BasicProtocol(BasicProtocol&&) = delete;
    BasicProtocol& operator=(BasicProtocol&&) = delete;

    void open() {}
    void close() {}
        
    // do nothing
    template<typename ConnT>
    void async_handshake(ConnT& conn, tb::DoneSlot done) {
        done({});
    }
    
    /// do nothing
    template<typename ConnT, typename PacketT>
    void async_process(ConnT& conn, const PacketT& packet, tb::DoneSlot done) { 
        done({});
    }

    /// as binary
    template<typename ConnT, typename MessageT>
    void async_write(ConnT& conn, const MessageT& m, tb::SizeSlot done) {
        async_write(conn, tb::ConstBuffer{&m, m.length()}, done);
    }
    template<typename ConnT>
    void async_write(ConnT& conn, tb::ConstBuffer buf, tb::SizeSlot done) {
        conn.async_write(buf, done);
    }
    template<typename ConnT>
    void async_read(ConnT& conn, tb::MutableBuffer buf, tb::SizeSlot done) {
        conn.async_read(buf, done);
    }
    template<typename ConnT, typename MessageT>
    void async_read(ConnT& conn, MessageT& m, tb::SizeSlot done) {
        conn.async_read(tb::MutableBuffer{&m, m.length()}, done);
    }
};

using DgramConn = Conn<tb::DgramSocket>;

using McastConn = Conn<tb::McastSocket>;

} // ft::io