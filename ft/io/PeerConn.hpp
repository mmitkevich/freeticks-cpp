#pragma once
#include <ft/utils/Common.hpp>
#include "ft/capi/ft-types.h"
#include "ft/core/Requests.hpp"
#include "ft/io/Conn.hpp"

namespace ft::io {

// Peer is logical remote counterparty connected using single connection. 
template<typename ProtocolT,  typename SubT, typename SocketT, typename ReactorT>
class PeerConn : public BasicConn<PeerConn<ProtocolT, SubT, SocketT, ReactorT>, SocketT, ReactorT> {
public:
    using Base = BasicConn<PeerConn<ProtocolT, SubT, SocketT, ReactorT>, SocketT, ReactorT>;
    using This = PeerConn<ProtocolT, SubT, SocketT, ReactorT>;
    using Protocol = ProtocolT;
    using typename Base::Socket;
    using typename Base::Reactor;
    using typename Base::Packet;
    using Connection = Base;
    using Subscription = SubT;
    using Base::open, Base::socket, Base::rbuf, Base::buffer_size, Base::do_read;
public:
    using Base::Base;

    /* overrides */
    void do_process(Packet& pkt) {
        protocol_.on_packet(pkt);
        Base::on_processed({});
    }

    Subscription& subscription() { return sub_; }
    
protected:
    Protocol protocol_;
    Subscription sub_;
};


}