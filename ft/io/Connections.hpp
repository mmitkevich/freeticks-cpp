#pragma once

#include "toolbox/io/Buffer.hpp"
#include "toolbox/io/ReactorHandle.hpp"
#include "toolbox/net/Protocol.hpp"
#include "toolbox/net/Endpoint.hpp"
#include "toolbox/net/Packet.hpp"
#include "toolbox/io/Reactor.hpp"

#include "toolbox/io/Socket.hpp"
#   include "toolbox/io/DgramSocket.hpp"
#   include "toolbox/io/McastSocket.hpp"

#include "ft/core/Parameters.hpp"

namespace ft::io {

namespace tb = toolbox;

template<typename SocketT, typename DerivedT>
class BasicConn : protected SocketT
, public core::BasicParameterized<DerivedT>
{
    using Base = SocketT;
    using This = BasicConn<SocketT, DerivedT>;
public:
    using Socket = SocketT;
    using Endpoint = typename Base::Endpoint;
    using PacketHeader = tb::PacketHeader<Endpoint>;
    using Packet = tb::Packet<PacketHeader, tb::ConstBuffer>;
public:
    using Base::Base;

    using Base::reactor;
    using Base::state;
    Packet& packet() { return packet_; } 
    const Packet& packet() const { return packet_; } 
    const Endpoint& endpoint() const { return packet().header().src(); } // "other side"
    void endpoint(const Endpoint& ep, std::string_view if_name={}) {
        packet().header().src(ep);
        packet().header().dst(ep);
        if_name_ = if_name;
    }
    
    const std::string& if_name() const { return if_name_; }

    bool is_open() const { return state() == tb::SocketState::Connected; }

    tb::Signal<const Packet&>& received() { return received_; }
    
    constexpr std::size_t buffer_size() { return 4096; };
    
    void open() {
        const auto& ep = endpoint();
        Base::open(ep.protocol());
        Base::bind(ep);
        Base::connect(ep);
        Base::recv(buffer_.prepare(buffer_size()), 0, tb::bind<&This::on_recv>(this));
    }
    void close() {
        Base::close();
    }
protected:
    void on_parameters_updated(const core::Parameters& params) {
        
    }

    /// Slots {
    void on_recv(ssize_t size, std::error_code ec) {
        packet_.header().recv_timestamp(tb::CyclTime::now().wall_time());
        buffer_.commit(size);
        packet_.buffer() = {buffer_.buffer().data(), (std::size_t)size};
        received_(packet_);
        buffer_.consume(size);
        Base::recv(buffer_.prepare(buffer_size()), 0, tb::bind<&This::on_recv>(this));
    }
    /// } Slots
protected:
    Packet packet_;
    tb::Buffer buffer_;
    tb::Signal<const Packet&> received_;
    std::string if_name_;
};

class DgramConn: public BasicConn<tb::DgramSocket, DgramConn> {
    using Base = BasicConn<tb::DgramSocket, DgramConn>;
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
    using Base::reactor;
};

class McastConn: public BasicConn<tb::McastSocket, McastConn> {
    using Base = BasicConn<tb::McastSocket, McastConn>;
public:
    using Endpoint = typename Base::Endpoint;
    using Packet = typename Base::Packet;
    using Socket = typename Base::Socket;
public:
    using Base::Base;
    McastConn(McastConn&&)  = default;

    using Base::received;
    using Base::state;
    using Base::reactor;
    using Base::packet;
    using Base::endpoint;
    using Base::if_name;    
    
    void open() {
        Base::open();
        std::error_code ec {};
        if(!if_name().empty()) {
            Socket::join_group(endpoint().address(), if_name().data());
            TOOLBOX_INFO << "join_group '"<<endpoint()<<"', if:'"<<if_name_;   
        } else {
            Socket::join_group(endpoint().address(), 0u);
            TOOLBOX_INFO << "join_group '"<<endpoint()<<"', if:0";               
        }
    }
    void close() {
        std::error_code ec {}; // ignored
        if(!if_name().empty()) {
            Socket::leave_group(endpoint().address(), if_name().data());
            TOOLBOX_INFO << "leave_group '"<<endpoint()<<"', if:'"<<if_name()<<"'";
        } else {
            Socket::leave_group(endpoint().address(), 0u);
            TOOLBOX_INFO << "leave_group '"<<endpoint()<<"', if:0";
        }
        Base::close();
    }
};

}