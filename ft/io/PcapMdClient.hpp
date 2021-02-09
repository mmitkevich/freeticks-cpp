#pragma once
#include "ft/core/Component.hpp"
#include "ft/core/Identifiable.hpp"
#include "ft/core/Instrument.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/StreamStats.hpp"
#include "ft/io/Protocol.hpp"
#include "ft/utils/Common.hpp"
#include "ft/utils/StringUtils.hpp"
#include "ft/core/Client.hpp"

#include "toolbox/net/Endpoint.hpp"
#include "toolbox/net/EndpointFilter.hpp"
#include "toolbox/net/Pcap.hpp"
#include "toolbox/sys/Log.hpp"
#include "toolbox/sys/Time.hpp"
//#include <netinet/in.h>
#include "ft/io/Service.hpp"
#include "ft/core/EndpointStats.hpp"

namespace ft::io {

class PcapConn : public Component, public tb::PcapDevice {
public:
    using Component::Component;
};

template<template<class...> class ProtocolM>
class PcapMdClient  : public BasicService<PcapMdClient<ProtocolM>, io::Service>
, public ProtocolM<PcapMdClient<ProtocolM>>
{
public:
    using Self = PcapMdClient<ProtocolM>;
    using Base = BasicService<PcapMdClient<ProtocolM>, io::Service>;
    using Protocol = ProtocolM<Self>;
    friend Protocol;
    using Stats = core::EndpointStats<tb::IpEndpoint>;
    using BinaryPacket = tb::PcapPacket;
    using typename Base::Reactor;
    using Peer = PcapConn;
public:
    explicit PcapMdClient(Reactor* r, Component* p)
    : Base(r,p)
    {
        peer_.packets().connect(tb::bind<&Self::on_packet_>(this));
    }
    Peer& peer() { return peer_; }
    
    auto& gw_stats() { return stats_; } 

    // dispatch parameters
    void on_parameters_updated(const core::Parameters& params) {
        auto pcap_pa = params["pcap"];

        pcap_pa["remote"].copy(inputs_);

        Protocol::on_parameters_updated(params);
        
        filter(pcap_pa["filter"]); // FIXME: get filter from streams automatically?
    }
    
    void filter(const core::Parameters& params) {
        // if protocols configuration not specified all protocols will be used
        // protocols supported are only tcp and udp
        if(params["protocols"].is_null()) {
            filter_.tcp = true;
            filter_.udp = true;
        } else {
            for(auto &p: params["protocols"]) {
                if(p == "udp")
                    filter_.udp = true;
                else if(p == "tcp")
                    filter_.tcp = true;
            }
        }
        params["dst"].copy(filter_.destinations);
        params["src"].copy(filter_.sources);
    }

    void open() {
        Protocol::open();
        run(); // FIXME: reactor.defer(&This::run);
    }
    void run() {
        for(auto& input: inputs_) {
            TOOLBOX_INFO<<"pcap replay start: "<<input;
            peer_.input(input);
            peer_.run();
            TOOLBOX_INFO<<"pcap replay done: "<<input;
        };
    }
    void close() {
        Protocol::close();
    }
    void report(std::ostream& os) {
        //protocol_.stats().report(os);
        stats_.report(os);
    }
    void on_idle() {
        report(std::cerr);
    }
private:
    void on_packet_(const tb::PcapPacket& pkt) {
        switch(pkt.header().protocol().protocol()) {
            case IPPROTO_TCP: case IPPROTO_UDP: {
                stats_.on_received(pkt);
                if(filter_(pkt.header())) {
                    Protocol::async_handle(peer_, pkt, tb::bind([this](std::error_code ec) { 
                    })); // FIXME: sync_process?
                    on_idle();      
                } else {
                    stats_.on_rejected(pkt);
                }
        } break;
            default: break; // ignore
        }
    }
private:
    Peer peer_;
    Stats stats_;
    toolbox::EndpointsFilter filter_;
    std::vector<std::string> inputs_;
};

} // ft::pcap