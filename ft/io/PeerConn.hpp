#pragma once
#include <ft/utils/Common.hpp>
#include "ft/capi/ft-types.h"
#include "ft/core/Requests.hpp"
#include "ft/io/Conn.hpp"

namespace ft::io {

// Peer is logical remote counterparty connected using single Connection via Protocol
template<typename ProtocolT,  typename SubscriptionT, typename SocketT, typename ReactorT>
class PeerConn : public BasicConn< 
    PeerConn<ProtocolT, SubscriptionT, SocketT, ReactorT>
    , SocketT, ReactorT>
{
    using This = PeerConn<ProtocolT, SubscriptionT, SocketT, ReactorT>;
    using Base = BasicConn<PeerConn<ProtocolT, SubscriptionT, SocketT, ReactorT>, SocketT, ReactorT>;    
public:
    using Protocol = ProtocolT;
    using typename Base::Socket;
    using typename Base::Reactor;
    using typename Base::Packet;
    using Connection = Base;
    using Subscription = SubscriptionT;
    using Base::open, Base::socket, Base::rbuf, Base::buffer_size, Base::do_recv;
public:
    using Base::Base;
    
    void on_recv(Packet &pkt, tb::DoneSlot done) {
        protocol_.on_packet(pkt);
        done({});
    }

    Subscription& subscription() { return sub_; }
    
protected:
    Protocol protocol_;
    Subscription sub_;
};


}