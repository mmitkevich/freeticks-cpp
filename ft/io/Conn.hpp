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

template<typename DerivedT, typename SocketT, typename ReactorT>
class BasicConn : public core::BasicComponent<core::StreamState>
, public io::BasicHeartbeats<DerivedT>
{
    using This = BasicConn<DerivedT, SocketT, ReactorT>;    
    using Base = core::BasicComponent<core::StreamState>;
    using Self = DerivedT;
    auto& self() { return static_cast<DerivedT*>(this); }
    const auto& self() const { return static_cast<const DerivedT*>(this); }
public:
    using Socket = SocketT;
    using Endpoint = typename Socket::Endpoint;
    using Transport = typename Socket::Protocol;
    using Packet = tb::Packet<Endpoint>;
    using Stats = core::EndpointStats<tb::BasicIpEndpoint<Transport>>;
    using Reactor = ReactorT;
public:
    BasicConn() = default;

    BasicConn(Reactor* reactor, SocketT&& socket)
    : reactor_(reactor) 
    , socket_(std::move(socket))
    {}
    BasicConn(const BasicConn&) = delete;
    BasicConn& operator=(const BasicConn&) = delete;

    BasicConn(BasicConn&&) = default;
    BasicConn& operator=(BasicConn&&) = default;
    
    ~BasicConn() {
        close();
    }
    
    using Base::state;

    /// attach to socket
    void open(Reactor* reactor) {
        reactor_ = reactor;
        socket_.open(*reactor_, Transport::v4());
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
        socket_.close();
    }
    
    /// attached socket
    Socket& socket() { return socket_; }
    const Socket& socket() const { return socket_; }
    Reactor& reactor() { return *reactor_; }
    const Reactor& reactor() const { return *reactor_; }

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

    /// last received packet
    Packet& last_recv() { return last_recv_; } 
    const Packet& last_recv() const { return last_recv_; } 

    /// configured remote endpoint
    const Endpoint& remote() const { return remote_; }
    void remote(const Endpoint& ep) { remote_ = ep; }

    /// configured local endpoint
    const Endpoint& local() const  { return local_; }
    void local(const Endpoint& ep) { local_ = ep; }
    
    /// configure connection using text url
    void remote(const std::string& url) {
        url_ = tb::ParsedUrl {url};
        Endpoint rep =  tb::TypeTraits<Endpoint>::from_string(url_.url());
        remote(rep);
        auto iface = url_.param("interface");
        if(!iface.empty()) {
            auto lep = tb::parse_ip_endpoint<Endpoint>(std::string_view(iface));
            local(lep);
        }
        last_recv().header().dst(rep);  // for filter
    }


    bool is_open() const { return state() == State::Open; }

    /// packet received
    tb::Slot<const Packet&, tb::Slot<std::error_code>>& received() { return received_slot_; }    

    /// connection stats
    Stats& stats() { return stats_; }

    bool can_write() const { return socket().can_write(); }
    bool can_read() const { return socket().can_read(); }

    template<typename MessageT>
    void async_write(const MessageT& m, tb::DoneSlot slot) {
        write_ = slot;
        socket().async_write(tb::ConstBuffer{&m, m.length()}, tb::bind<&This::on_write>(this));
    }
    void on_write(ssize_t size, std::error_code ec) {
        write_(ec);
        write_.reset();
    }

protected:
    constexpr static std::size_t RecvBufferSize = 4096;
    
    constexpr std::size_t buffer_size() { return RecvBufferSize; };
    tb::Buffer& rbuf() { return rbuf_; }   

    void on_recv(std::error_code ec) {
        if(!ec)
            self()->do_recv(ec);
    }

    // could be overriden
    void do_recv() {
        socket().async_read(rbuf_.prepare(buffer_size()), tb::bind<&Self::on_recv>(self()));    
    }

    void on_recv(ssize_t size, std::error_code ec) {
        if(!ec) {
            last_recv().header().recv_timestamp(tb::CyclTime::now().wall_time());
            recvsize_ = size;
            rbuf().commit(size);
            last_recv().buffer() = {rbuf().buffer().data(), (std::size_t)size};
            self()->on_recv(last_recv(), tb::bind<&Self::on_recv_done>(self()));
        } else {
            self()->on_recv_done(ec);
        }
    }   

    void on_recv(Packet &pkt, tb::DoneSlot done) {
        received_(pkt, tb::bind<&Self::on_recv_done>());
    }

    void on_recv_done(std::error_code ec) {
        if(ec.value()!=tb::unbox(std::errc::operation_would_block))
            rbuf_.consume(recvsize_);
        self()->do_recv();
    }

protected:
    Reactor* reactor_ {};
    std::size_t recvsize_ {};
    Socket socket_ {};
    Endpoint remote_;
    Endpoint local_;
    Packet last_recv_;
    tb::Buffer rbuf_;
    tb::Slot<const Packet&, tb::DoneSlot> received_slot_;
    Stats stats_;
    tb::ParsedUrl url_;
    tb::DoneSlot write_;
};

/// crtp multi bases for features like Heartbeated
template<typename SocketT, typename ReactorT>
class Conn : public BasicConn<Conn<SocketT, ReactorT>, SocketT, ReactorT>
{
    using Base = BasicConn<Conn<SocketT, ReactorT>, SocketT, ReactorT>;
public:
    using Base::Base;
};



class BasicProtocol {
public:
    void open() {}
    void close() {}
};

template<typename ReactorT>
using DgramConn = Conn<tb::DgramSocket, ReactorT>;

template<typename ReactorT>
using McastConn = Conn<tb::McastSocket, ReactorT>;

} // ft::io