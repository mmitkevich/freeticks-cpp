#pragma once

#include "ft/core/Component.hpp"
#include "ft/core/StreamStats.hpp"
#include "toolbox/util/ByteTraits.hpp"
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
#include "ft/core/Subscription.hpp"
#include "toolbox/net/ParsedUrl.hpp"
#include "toolbox/net/Sock.hpp"
#include "toolbox/sys/Time.hpp"
#include "toolbox/util/Slot.hpp"
#include "ft/io/Heartbeats.hpp"
#include "ft/io/Service.hpp"
#include "ft/io/Protocol.hpp"
#include "toolbox/util/ByteTraits.hpp"
#include <boost/ref.hpp>

namespace ft::io {

/// Conn<Socket,Parent> = Component<Parent> 
template<class Self, class SocketT, class ParentT> 
class BasicConn
: public BasicComponent<Self, ParentT>
, public BasicHeartbeats<Self> // feature mixin
{
    FT_MIXIN(Self);
    using Base = BasicComponent<Self, ParentT>;
  public:
    using Socket = typename boost::unwrap_reference<SocketT>::type;
    using typename Base::Parent;
    using Reactor = typename Parent::Reactor;
    using Endpoint = typename Socket::Endpoint;
    using Transport = typename Socket::Protocol;
    using Packet = tb::Packet<tb::ConstBuffer, Endpoint>;
    using Stats = core::EndpointStats<tb::BasicIpEndpoint<Transport>>;
    using Subscription = core::Subscription;
  public:
    using Base::parent;
    using Base::Base;

    BasicConn() = default;
    BasicConn(const BasicConn&) = delete;
    BasicConn&operator=(const BasicConn&) = delete;
    BasicConn(BasicConn&&) = default;
    BasicConn& operator=(BasicConn&&) = default;
    
    explicit BasicConn(std::string_view url, Parent* parent=nullptr)
    : Base(parent) { 
        self()->url(url);
    }

    explicit BasicConn(SocketT&& socket, const Endpoint& remote, Parent* parent=nullptr)
    :  Base(parent)
    ,  socket_(std::move(socket))
    ,  remote_(remote) {}

    ~BasicConn() {
        close();
    }

    void transport(Transport transport) {
        transport_ = transport;
    }

    void open() {
        socket_.open(*reactor(), transport_);
        if constexpr (tb::SocketTraits::is_mcast<Socket>) {
            socket_.bind(remote());
            socket_.join_group(remote());
        } else {
            if(local()!=Endpoint())
                socket_.bind(local());
        }
    }
    
    void close() {
        if(!socket().get()) {
            if constexpr (tb::SocketTraits::is_mcast<Socket>) {
                socket_.leave_group(remote());
            }
            socket().close(); // tcp disconnect
        }
    }
    
    /// attached socket
    Socket& socket() { return socket_; }
    const Socket& socket() const { return socket_; }
    
    Reactor* reactor() { assert(parent()); return parent()->reactor(); }

    /// connect  attached socket via async_connect if supported
    void async_connect(tb::DoneSlot slot) { 
        if constexpr (tb::SocketTraits::is_stream<SocketT>) {
            socket().async_connect(remote(), slot);
        } else {
            // nothing to do
            slot({});
        }
    }
    
    static bool supports(std::string_view transport) { return transport == Transport::name(); }

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
        remote_ =  tb::TypeTraits<Endpoint>::from_string(url);
        auto iface = url_.param("interface");
        if(!iface.empty()) {
            local_ = tb::parse_ip_endpoint<Endpoint>(iface);
        }
    }

    // subscription handled by connection
    const Subscription& subscription() const  { return sub_; }
    Subscription& subscription() { return sub_; }


    /// connection stats
    Stats& stats() { return stats_; }

    bool can_write() const { return socket().can_write(); }
    bool can_read() const { return socket().can_read(); }

    /// use appropriate socket write operation according to SocketTraits
    void async_write(tb::ConstBuffer buf, tb::SizeSlot slot) {
        if constexpr(tb::SocketTraits::is_dgram<Socket>) { // FIXME: tb::SocketTraits<Socket>::is_dgram
            // udp
            socket().async_sendto(buf, remote(), slot);
        } else {
            // tcp
            socket().async_write(buf, slot);
        }
    }
    
    /// use appropriate socket read operation according to SocketTraits
    void async_read(tb::MutableBuffer buf, tb::SizeSlot slot) {
        packet_.buffer() = buf;
        if constexpr(tb::SocketTraits::is_mcast<Socket>) {
            // for mcast dst = mcast addr
            packet_.header().dst() = remote();
            socket().async_recvfrom(buf, 0, packet_.header().src(), slot); // fills src
        } else if constexpr(tb::SocketTraits::is_dgram<Socket>) {
            packet_.header().dst() = local();
            socket().async_recvfrom(buf, 0, packet_.header().src(), slot); // fills src
        } else { // tcp/stream fallback
            packet_.header().dst() = local();
            packet_.header().src() = remote();
            socket().async_read(buf, slot);
        }
    }

    /// last packet
    Packet& packet() { return packet_; }
    
    /// read buffer size
    constexpr std::size_t buffer_size() { return 4096; };
    tb::Buffer& rbuf() { return rbuf_; }   

    /// loop
    template<typename HandlerT>
    void run() {
        //TOOLBOX_DUMP_THIS<<"rbuf.prepare "<< self()->buffer_size();
        self()->async_read(rbuf().prepare(self()->buffer_size()), 
            tb::bind([this](ssize_t size, std::error_code ec) {
                if(!ec) {
                    assert(size>=0);
                    //TOOLBOX_DUMP_THIS<<"rbuf.commit "<< size;
                    rbuf().commit(size);
                    // replace total buffer size with real received data size
                    packet_.buffer() = typename Packet::Buffer {packet_.buffer().data(), (size_t) size}; 
                    packet_.header().recv_timestamp(tb::WallClock::now());
                    // handle
                    HandlerT* handler = static_cast<HandlerT*>(parent());
                    handler->async_handle(*self(), packet_, tb::bind([this](std::error_code ec) {
                        auto size = packet().buffer().size(); 
                        //TOOLBOX_DUMP_THIS<<"rbuf.consume "<< size;
                        rbuf().consume(size);
                        if(!ec) {
                            // loop
                            self()->template run<HandlerT>();
                        } else {
                            self()->on_error(ec);
                        }
                    }));
                } else {
                    self()->on_error(ec);
                }
            })
        );
    }

    void on_error(std::error_code ec) {
        TOOLBOX_ERROR << "error "<<ec<<" conn remote="<<remote_<<" local="<<local_;
    }
protected:
    Packet packet_;
    SocketT socket_ {};
    Stats stats_;
    tb::ParsedUrl url_;
    tb::DoneSlot write_;
    Endpoint local_, remote_;
    tb::Buffer rbuf_;
    Subscription sub_;
    Transport transport_ = Transport::v4();
};

template<class SocketT, class ParentT>
class Conn: public BasicConn<Conn<SocketT, ParentT>, SocketT, ParentT> {
    using Base = BasicConn<Conn<SocketT, ParentT>, SocketT, ParentT>;
    using Base::Base;
};

template<class SocketT, class ParentT>
class ServerConn: public BasicConn<Conn<SocketT, ParentT>, boost::reference_wrapper<SocketT>, ParentT> {
    using Base = BasicConn<Conn<SocketT, ParentT>, boost::reference_wrapper<SocketT>, ParentT>;
    using Base::Base;
};

} // ft::io