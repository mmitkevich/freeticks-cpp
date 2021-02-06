#pragma once

#include "ft/core/Component.hpp"
#include "ft/core/StreamStats.hpp"
#include "toolbox/util/ByteTraits.hpp"
#include "toolbox/io/Buffer.hpp"
#include "toolbox/io/PollHandle.hpp"
#include "toolbox/net/DgramSock.hpp"
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
#include "ft/io/Protocol.hpp"
#include "toolbox/util/ByteTraits.hpp"
#include "ft/utils/Compat.hpp"

namespace ft::io {

template<class Self, class SocketT, class ParentT, typename...> 
class BasicConn : public EnableParent<ParentT, io::BasicReactiveComponent<typename ParentT::Reactor>>
, public BasicHeartbeats<Self> // feature mixin
{
    FT_SELF(Self);
    using Base = EnableParent<ParentT, io::BasicReactiveComponent<typename ParentT::Reactor>>;
  public:
    using SocketRef = SocketT;
    using Socket = std::decay_t<util::unwrap_reference_t<SocketT>>;
    using typename Base::Reactor;
    using typename Base::Parent;
    using Endpoint = typename Socket::Endpoint;
    using Transport = typename Socket::Protocol;
    using Packet = tb::Packet<tb::ConstBuffer, Endpoint>;
    using Stats = core::EndpointStats<tb::BasicIpEndpoint<Transport>>;
    //using Subscription = core::Subscription;
  public:
    using Base::parent, Base::reactor;
    using Base::Base;
    
    BasicConn(BasicConn&&)=default;
    BasicConn&operator=(BasicConn&&)=default;

    BasicConn(std::string_view url, Parent* parent=nullptr, Identifier id={})
    : Base(parent->reactor(), parent, id) {
        TOOLBOX_DUMPV(5)<<"self:"<<self()<<" parent:"<<parent;
        self()->url(url);
    }

    BasicConn(SocketRef&& socket, Parent* parent=nullptr, Identifier id={})
    :  Base(parent->reactor(), parent)
    ,  socket_(std::move(socket))
    {
        TOOLBOX_DUMPV(5)<<"self:"<<self()<<" parent:"<<parent;
    }

    BasicConn(Parent* parent, Identifier id={})
    : Base(parent->reactor(), parent, id) {
        TOOLBOX_DUMPV(5)<<"self:"<<self()<<" parent:"<<parent;
    }

    ~BasicConn() {
        close();
    }

    
    void transport(Transport transport) {
        transport_ = transport;
    }

    void socket(SocketRef&& socket) {
        socket_ = std::move(socket);
    }

    void open() {
        socket().open(*reactor(), transport_);
        if constexpr (tb::SocketTraits::is_mcast<Socket>) {
            socket().bind(remote());
            socket().join_group(remote());
        } else {
            if(local()!=Endpoint())
                socket().bind(local());
        }
    }
    
    void close() {
        if(!socket().get()) {
            if constexpr (tb::SocketTraits::is_mcast<Socket>) {
                socket().leave_group(remote());
            }
            socket().close(); // tcp disconnect
        }
    }
    
    /// attached socket
    Socket& socket() { return socket_; }
    const Socket& socket() const { return socket_; }
    
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
    Endpoint& remote() { 
        if constexpr(tb::SocketTraits::is_mcast<Socket>) {
            return packet_.header().dst();
        } else {
            return packet_.header().src();
        }
    }
    const Endpoint& remote() const { 
        if constexpr(tb::SocketTraits::is_mcast<Socket>) {
            return packet_.header().dst();
        } else {
            return packet_.header().src();
        }
    }
    
    /// configure connection using text url
    void url(std::string_view url) {
        url_ = tb::ParsedUrl {url};        
        remote() =  tb::TypeTraits<Endpoint>::from_string(url);
        auto iface = url_.param("interface");
        if(!iface.empty()) {
            local() = tb::parse_ip_endpoint<Endpoint>(iface);
        }
        TOOLBOX_INFO<<"Peer remote:"<<remote()<<", local:"<<local()<<", url="<<url;
    }
    
    core::Subscription& subscription() { return sub_;}

    // subscription handled by connection
    //const Subscription& subscription() const  { return sub_; }
    //Subscription& subscription() { return sub_; }

    /// connection stats
    Stats& stats() { return stats_; }

    bool can_write()  { return socket().can_write(); }
    bool can_read()  { return socket().can_read(); }

    /// could enqueue (could allocate)
    void async_write(tb::ConstBuffer buf, tb::SizeSlot slot) {
        if(can_write()) {
            if(!wqueue_.empty())
                slot = tb::bind<&Self::on_write>(self()); // got some queue
            self()->async_write_nq(buf, slot);
        } else {
            // serialize data
            auto wb = wbuf_.prepare(buf.size());
            std::memcpy(wb.data(), buf.data(), buf.size());
            wqueue_.emplace_back(buf.size(), tb::bind<&Self::on_write>(self())); // part size + slot
        }
    }

    void on_write(ssize_t size, std::error_code ec) {
        // check queue
        assert(!wqueue_.empty());
        auto [wsize, done]  = wqueue_.front();
        wqueue_.pop_front();
        async_write_nq({wbuf_.buffer().data(), wsize}, done);
    }

    /// async_write no queueing
    void async_write_nq(tb::ConstBuffer buf, tb::SizeSlot slot) {
        assert(can_write());
        // immediate write
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

    template<typename HandlerT>
    void run() {
        TOOLBOX_DUMP<<"Conn::async_read self="<<self()<<", rbuf(size="<< self()->buffer_size()<<"), local:"<<local()<<", remote:"<<remote();
        self()->async_read(rbuf().prepare(self()->buffer_size()), tb::bind(
        [this](ssize_t size, std::error_code ec) {
            if(!ec) {
                assert(size>=0);
                rbuf().commit(size);
                // replace total buffer size with real received data size
                packet_.buffer() = typename Packet::Buffer {packet_.buffer().data(), (size_t) size}; 
                packet_.header().recv_timestamp(tb::WallClock::now());
                TOOLBOX_DUMP<<"Conn::recv self:"<<self()<<", size:"<<size<<
                    " local:"<<local()<<", remote:"<<remote()<<", ec:"<<ec<<" pkt hdr:"<<packet_.header();
                HandlerT{}(*self(), packet_, tb::bind([this](std::error_code ec) {
                    auto size = packet().buffer().size(); 

                    rbuf().consume(size);
                    if(!ec) {
                        self()->template run<HandlerT>();
                    } else {
                        self()->on_error(ec);
                    }
                }));
            } else {
                self()->on_error(ec);
            }
        }));
    }
    void on_error(std::error_code ec) {
        TOOLBOX_ERROR << "error "<<ec<<" conn remote="<<remote()<<" local="<<local();
    }
protected:
    tb::CompletionSlot<std::error_code> handle_;
    core::Subscription sub_;
    Packet packet_;
    SocketRef socket_;
    Stats stats_;
    tb::ParsedUrl url_;
    tb::DoneSlot write_;
    Endpoint local_;
    tb::Buffer rbuf_;
    tb::Buffer wbuf_;
    std::deque<std::tuple<std::size_t,tb::SizeSlot>> wqueue_;
    //Subscription sub_;
    Transport transport_ = Transport::v4();
};

template<class SocketT, class ParentT=io::BasicReactiveComponent<io::Reactor>, typename... ArgsT>
class Conn: public BasicConn<
    Conn<SocketT, ParentT> // This
    , SocketT, ParentT, ArgsT...>
{
    using Base = BasicConn<
        Conn<SocketT, ParentT>, 
        SocketT, ParentT>;
    using Base::Base;
};

using DgramConn = Conn<tb::DgramSock>;
using McastConn = Conn<tb::McastSock>;

template<
  class SocketT
, class ParentT = io::BasicReactiveComponent<io::Reactor>
, typename...ArgsT>
class ServerConn: public BasicConn<
  ServerConn<SocketT, ParentT>          // This
, std::reference_wrapper<SocketT>     // not-owner
, ParentT>
{
    using Base = BasicConn<
        ServerConn<SocketT, ParentT> // This
        , std::reference_wrapper<SocketT>
        , ParentT>;
    using Base::Base;
};

} // ft::io