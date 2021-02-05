#pragma once
#include "ft/core/Component.hpp"
#include "ft/core/Identifiable.hpp"
#include "ft/core/Instrument.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/StreamStats.hpp"
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

template<class ProtocolT>
class PcapMdClient  : public BasicService<PcapMdClient<ProtocolT>>
{
public:
    using This = PcapMdClient<ProtocolT>;
    using Base = BasicService<PcapMdClient<ProtocolT>>;
    using Protocol = ProtocolT;
    using Stats = core::EndpointStats<tb::IpEndpoint>;
    using BinaryPacket = tb::PcapPacket;
    using typename Base::Reactor;
    using Peer = PcapConn;
public:
    template<typename...ArgsT>
    explicit PcapMdClient(Reactor* r, Component* p, ArgsT...args)
    : Base(r,p)
    , protocol_(std::forward<ArgsT>(args)...)
    {
        peer_.packets().connect(tb::bind<&PcapMdClient::on_packet_>(this));
    }
    Protocol& protocol() {   return protocol_; }
    Peer& peer() { return peer_; }
    
    auto& stats() { return protocol_.stats(); }
    auto& gw_stats() { return stats_; } 

    // dispatch parameters
    void on_parameters_updated(const core::Parameters& params) {
        auto pcap_pa = params["pcap"];

        pcap_pa["urls"].copy(inputs_);

        auto streams_p = params["connections"];
        protocol_.on_parameters_updated(streams_p);
        
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
        protocol_.open();
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
        protocol_.close();
    }
    void report(std::ostream& os) {
        //protocol_.stats().report(os);
        stats_.report(os);
    }
    void on_idle() {
        report(std::cerr);
    }
    core::Stream& stream(core::StreamTopic topic) {
       return protocol().stream(topic);
    }
private:
    void on_packet_(const tb::PcapPacket& pkt) {
        switch(pkt.header().protocol().protocol()) {
            case IPPROTO_TCP: case IPPROTO_UDP: {
                stats_.on_received(pkt);
                if(filter_(pkt.header())) {
                    protocol_.async_handle(peer_, pkt, tb::bind([this](std::error_code ec) { 
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
    Protocol protocol_;
    Peer peer_;
    Stats stats_;
    toolbox::EndpointsFilter filter_;
    std::vector<std::string> inputs_;
};

} // ft::pcap