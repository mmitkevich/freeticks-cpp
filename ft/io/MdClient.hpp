#pragma once
#include "ft/core/Requests.hpp"
//#include "ft/io/Routing.hpp"
#include "ft/io/Service.hpp"
#include "ft/utils/Common.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/Component.hpp"
#include "ft/core/Client.hpp"
#include "toolbox/io/Socket.hpp"
#include "toolbox/net/EndpointFilter.hpp"

#include "toolbox/sys/Time.hpp"
#include "toolbox/util/Slot.hpp"
#include "toolbox/util/String.hpp"

#include "ft/io/Client.hpp"
#include "ft/io/Conn.hpp"

#include "ft/core/Tick.hpp"
#include "ft/core/Instrument.hpp"

#include <boost/mp11/detail/mp_list.hpp>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <vector>

namespace ft::io {

template< class Self
, template<class...> class ProtocolM
, class PeerT
, typename StateT
, template<class...> class IdleTimerM = BasicIdleTimer // mixin
> class BasicMdClient : public io::BasicClient<Self, PeerT, StateT>
, public IdleTimerM<Self>
, public ProtocolM<Self>
{
    FT_SELF(Self);
    
    using Base = BasicClient<Self, PeerT, StateT>;
  //  using Router = RouterT;
    using IdleTimer = IdleTimerM<Self>;
    using Protocol = ProtocolM<Self>;
public:
    using typename Base::Peer;
    using typename Base::PeersMap;
    using typename Base::Packet;
    using typename Base::Socket;
    using typename Base::Endpoint;
    using typename Base::State;
public:
    using Base::Base;
    using Base::state, Base::reactor;
    using Base::peers, Base::for_each_peer;
    using Base::async_connect;
    using Base::async_write;
    using Protocol::stats;
    using Protocol::async_handle, Protocol::async_write;
    using typename Protocol::BestPriceSignal;
    using typename Protocol::InstrumentSignal;
    

    void open() {
        Base::open();
        IdleTimer::open();
        Protocol::open();
    }

    void close() {
        Protocol::close();
        IdleTimer::close();
        Base::close();
    }

    void on_parameters_updated(const core::Parameters& params) {
        Base::on_parameters_updated(params);        
        Protocol::on_parameters_updated(params);
        IdleTimer::on_parameters_updated(params);
    }

    void on_idle() {
        std::stringstream ss;
        stats().report(ss);
        TOOLBOX_INFO << ss.str();
    }
protected:

    //Router router_{self()};
}; // BasicMdClient

template<template<class...> class ProtocolTT, class PeerT, typename StateT=core::State>
class MdClient : public BasicMdClient<MdClient<ProtocolTT, PeerT, StateT>, ProtocolTT, PeerT, StateT>
{
    using Base = BasicMdClient<MdClient<ProtocolTT, PeerT, StateT>, ProtocolTT, PeerT, StateT>;
  public:
    using Base::Base;
};

} // ft::io