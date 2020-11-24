#include "toolbox/io/Buffer.hpp"
#include "toolbox/io/Poller.hpp"
#include "toolbox/net/Protocol.hpp"
#include "toolbox/net/Endpoint.hpp"
#include "toolbox/net/Packet.hpp"
#include "toolbox/io/Reactor.hpp"
#include "toolbox/net/McastSock.hpp"

#include "ft/io/Conn.hpp"

namespace ft::io {

namespace tb = toolbox;

template<typename ProtocolT, typename SocketT>
class BasicDgramConn : public BasicConn {
public:
    using Base = BasicConn;
    using Endpoint = tb::BasicIpEndpoint<ProtocolT>;
    using PacketHeader = tb::PacketHeader<Endpoint>;
    using Packet = tb::Packet<PacketHeader, tb::ConstBuffer>;
    using Socket = SocketT;
    using This = BasicDgramConn<ProtocolT, SocketT>;
public:
  BasicDgramConn(tb::Reactor& reactor, Endpoint src, Endpoint dst)
    : Base(reactor)
    , packet_(PacketHeader(src, dst))
    {}

public:
    const Packet& packet() const { return packet_; } 

public:
    /// Connection {
    const Endpoint& endpoint() const { return packet().header().src(); } // "other side"
    Socket& socket() { return socket_; }
    bool is_connected() const { return !socket_.empty(); }
    tb::Signal<const Packet&>& received() { return received_; }
    void connect() {
        assert(!is_connected());
        std::error_code ec {};
        socket_ = Socket(endpoint().protocol(), ec);
        if(ec)
            throw std::system_error{tb::make_sys_error(ec.value()), "BasicDgramConn::connect::socket"};
        socket_.bind(endpoint(), ec);
        TOOLBOX_INFO << "bind '"<<endpoint()<<"', ec:"<<ec;
        assert(!socket_.empty());
        io_handle_ = reactor().subscribe(socket_.get(), tb::PollEvents::Read, tb::bind<&This::on_io_event>(this));
        if(ec)
            throw std::system_error{tb::make_sys_error(ec.value()), "BasicDgramConn::connect::subscribe"};
    }
    void disconnect() {
        socket_.close();
        socket_ = Socket();
        assert(socket_.empty());
        io_handle_.reset();
    }
    /// } Connection
protected:
    /// Slots {
    void on_io_event(tb::CyclTime now, tb::os::FD fd, tb::PollEvents events) {
        packet_.header().recv_timestamp(now.wall_time());
        std::size_t size = socket_.recv(buffer_.prepare(4096), 0);
        buffer_.commit(size);
        packet_.buffer() = buffer_.buffer();
        received_(packet_);
        buffer_.consume(size);
    }
    /// } Slots
protected:
    Packet packet_;
    tb::Buffer buffer_;
    tb::Signal<const Packet&> received_;
    Socket socket_;    
    tb::Reactor::Handle io_handle_;
};
using DgramConn = BasicDgramConn<tb::UdpProtocol, tb::McastSock>;

template<typename ProtocolT, typename SocketT>
class BasicMcastDgramConn: public BasicDgramConn<ProtocolT, SocketT> {
public:
    using Base = BasicDgramConn<ProtocolT, SocketT>;
    using This = BasicMcastDgramConn<ProtocolT, SocketT>;
    using Endpoint = typename Base::Endpoint;
    using Packet = typename Base::Packet;
    using Socket = typename Base::Socket;
public:
    BasicMcastDgramConn(toolbox::Reactor& reactor, Endpoint ep, std::string_view if_name)
    : Base(reactor, ep, ep)
    , if_name_(if_name)
    {}
    
    /// Packet {
    using Base::packet;
    /// } Packet

    /// Connection {
    using Base::reactor;
    using Base::endpoint;
    using Base::is_connected;
    using Base::socket_;
    void connect() {
        Base::connect();
        std::error_code ec {};
        if(!if_name_.empty()) {
            socket_.join_group(endpoint().address(), if_name_.data(), ec);
            TOOLBOX_INFO << "join_group '"<<endpoint()<<"', if:'"<<if_name_<<"', ec:"<<ec;                
        } else {
            socket_.join_group(endpoint().address(), 0u, ec);
            TOOLBOX_INFO << "join_group '"<<endpoint()<<"', if:0, ec:"<<ec;                
        }
        if(ec)
            throw std::system_error{tb::make_sys_error(ec.value()), "BasicMcastDgramConn::connect::join_group"};            
    }
    void disconnect() {
        assert(is_connected());
        std::error_code ec {}; // ignored
        if(!if_name_.empty()) {
            socket_.leave_group(endpoint().address(), if_name_.data(), ec);
            TOOLBOX_INFO << "leave_group '"<<endpoint()<<"', if:'"<<if_name_<<"', ec:"<<ec;                
        } else {
            socket_.leave_group(endpoint().address(), 0u, ec);
            TOOLBOX_INFO << "leave_group '"<<endpoint()<<"', if:0, ec:"<<ec;                
        }
        Base::disconnect();
    }
    /// }  
protected:
    std::string_view if_name_;
};

using McastDgramConn = BasicMcastDgramConn<tb::UdpProtocol, tb::McastSock>;

}