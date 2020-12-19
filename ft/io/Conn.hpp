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
#include "toolbox/net/Sock.hpp"
#include "toolbox/util/Slot.hpp"

namespace ft::io {

class IConnection {
public:

};

template<typename ProtocolT>
std::string_view protocol_name(const ProtocolT& protocol) { return {}; }

inline std::string_view protocol_schema(tb::UdpProtocol& protocol) { return "udp"; }
inline std::string_view protocol_schema(tb::McastProtocol& protocol) { return "mcast"; }


/// transforms incoming data into packets for various socket types
/// also handles automatic reconnect and maintains Stream state
/// multiple connections could share single socket
template<typename DerivedT, typename SocketT, typename ReactorT>
class BasicConn : public core::BasicComponent<core::StreamState>
{
    using This = BasicConn<DerivedT, SocketT, ReactorT>;    
    using Base = core::BasicComponent<core::StreamState>;
public:
    using Socket = SocketT;
    using Endpoint = typename Socket::Endpoint;
    using Protocol = typename Socket::Protocol;
    using PacketHeader = tb::PacketHeader<Endpoint>;
    using Packet = tb::Packet<PacketHeader, tb::ConstBuffer>;
    using Stats = core::EndpointStats<tb::BasicIpEndpoint<Protocol>>;
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
    void open(Reactor* reactor, Socket&& socket) {
        reactor_ = reactor;
        socket_ = std::move(socket);
    }

    void disconnect() {
        if constexpr(tb::SocketTraits::has_connect<SocketT>) {
            socket().disconnect(remote()); // leave_group
        }
    }    
    
    void close() {
        disconnect();
        socket_.close();
    }
    
    /// attached socket
    Socket& socket() { return socket_; }
    Reactor& reactor() { return *reactor_; }

    /// connect  attached socket via async_connect if supported
    void connect() { 
        if constexpr (tb::SocketTraits::has_connect<SocketT>) {
            socket().async_connect(remote(), tb::bind<&This::on_connect>(this));
        } else {
            on_connect({});
        }
    }
    
    static bool supports(std::string_view proto) { return true; }

    /// last received packet
    Packet& last_packet() { return last_packet_; } 
    const Packet& last_packet() const { return last_packet_; } 

    /// configured remote endpoint
    const Endpoint& remote() const { return remote_; }
    void remote(const Endpoint& ep) { remote_ = ep; }

    /// configured local endpoint
    const Endpoint& local() const  { return local_; }
    void local(const Endpoint& ep) { local_ = ep; }
    
    /// configure connection using text url
    void url(const std::string& url) {
        url_ = tb::ParsedUrl {url};
        Endpoint ep =  tb::TypeTraits<Endpoint>::from_string(url_.url());
        // TODO: what is really needed?
        remote(ep);
        local(ep);
        last_packet().header().dst(ep);  // for filter
    }

    bool is_open() const { return state() == State::Open; }

    /// packet received
    tb::Slot<const Packet&, tb::Slot<std::error_code>>& packet() { return packet_; }    

    /// connection stats
    Stats& stats() { return stats_; }

    template<typename MessageT>
    void async_write(const MessageT& m, tb::SizeSlot slot) {
        socket().async_write(tb::ConstBuffer{&m, m.length()}, slot);
    }

    DerivedT* impl() { return static_cast<DerivedT*>(impl); }
protected:
    constexpr static std::size_t RecvBufferSize = 4096;
    
    constexpr std::size_t buffer_size() { return RecvBufferSize; };
    tb::Buffer& rbuf() { return rbuf_; }   

    void on_connect(std::error_code ec) {
        if(!ec)
            impl()->do_read(ec);
    }

    // start read
    void do_read() {
        socket().async_read(rbuf_.prepare(buffer_size()), tb::bind<&DerivedT::on_recv>(this));    
    }

    // basic implementation via async_read
    void on_recv(ssize_t size, std::error_code ec) {
        if(!ec) {
            last_packet_.header().recv_timestamp(tb::CyclTime::now().wall_time());
            recvsize_ = size;
            rbuf().commit(size);
            last_packet_.buffer() = {rbuf().buffer().data(), (std::size_t)size};
            impl()->do_process(last_packet_);
        } else {
            on_processed(ec);
        }
    }   

    void do_process(Packet &pkt) {
        packet_(pkt, tb::bind<&DerivedT::on_processed>());
    }

    void on_processed(std::error_code ec) {
        if(ec.value()!=tb::unbox(std::errc::operation_would_block))
            rbuf_.consume(recvsize_);
        impl()->do_read();   // read next
    }

protected:
    Reactor* reactor_ {};
    std::size_t recvsize_ {};
    Socket socket_ {};
    Endpoint remote_;
    Endpoint local_;
    Packet last_packet_;
    tb::Buffer rbuf_;
    tb::Slot<const Packet&, tb::Slot<std::error_code>> packet_;
    Stats stats_;
    tb::ParsedUrl url_;
};

class DgramConn: public BasicConn<DgramConn, tb::DgramSock, tb::Reactor> {
    using Base = BasicConn<DgramConn, tb::DgramSock, tb::Reactor>;
public:
    using Base::Base;
    DgramConn() = default;
};

class McastConn: public BasicConn<McastConn, tb::McastSock, tb::Reactor> {
    using Base = BasicConn<McastConn, tb::McastSock, tb::Reactor>;
public:
    using Base::Base;
    McastConn() = default;
};

}