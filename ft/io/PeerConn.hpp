#pragma once
#include <ft/utils/Common.hpp>
#include "ft/capi/ft-types.h"
#include "ft/core/Requests.hpp"
#include "ft/io/Conn.hpp"
#include "ft/io/Service.hpp"

namespace ft::io {

/// Peer = Conn + Subscription
template<typename SubscriptionT, typename SocketT, typename ServiceT>
class BasicPeerConn : public BasicConn< 
    BasicPeerConn<SubscriptionT, SocketT, ServiceT>
    , SocketT, ServiceT>
{
    using This = BasicPeerConn<SubscriptionT, SocketT, ServiceT>;
    using Base = BasicConn<BasicPeerConn<SubscriptionT, SocketT, ServiceT>, SocketT, ServiceT>;    
public:
    using typename Base::Socket;
    using typename Base::Reactor;
    using typename Base::Packet;
    using Subscription = SubscriptionT;
    using Base::open, Base::socket;
public:
    using Base::Base;

    const Subscription& subscription() const  { return sub_; }
    Subscription& subscription() { return sub_; }
protected:
    Subscription sub_;
};

template<typename SocketT>
using PeerConn = BasicPeerConn<core::Subscriber, SocketT, io::Service>;

using DgramPeerConn = PeerConn<tb::DgramSocket>;
}