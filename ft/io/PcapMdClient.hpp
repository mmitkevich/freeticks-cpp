#pragma once
#include "ft/core/Component.hpp"
#include "ft/core/Instrument.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/StreamStats.hpp"
#include "ft/utils/Common.hpp"
#include "ft/utils/StringUtils.hpp"
#include "ft/core/MdClient.hpp"

#include "toolbox/net/Endpoint.hpp"
#include "toolbox/net/EndpointFilter.hpp"
#include "toolbox/net/Pcap.hpp"
#include "toolbox/sys/Log.hpp"
#include "toolbox/sys/Time.hpp"
//#include <netinet/in.h>

#include "ft/core/EndpointStats.hpp"

namespace ft::io {


template<typename ProtocolT>
class PcapMdClient : public core::BasicComponent<PcapMdClient<ProtocolT>, core::State> {
public:
    using This = PcapMdClient<ProtocolT>;
    using Base = core::BasicComponent<This, core::State>;
    using Protocol = ProtocolT;
    using Stats = core::EndpointStats<tb::IpEndpoint>;
    using BinaryPacket = tb::PcapPacket;
public:
    template<typename...ArgsT>
    explicit PcapMdClient(tb::Reactor* reactor, ArgsT...args)
    : Base(reactor)
    , protocol_(std::forward<ArgsT>(args)...)
    {
        device_.packets().connect(tbu::bind<&PcapMdClient::on_packet_>(this));
    }

    using Base::stop;

    Protocol& protocol() {   return protocol_; }
    toolbox::PcapDevice& device() { return device_; }
    
    auto& stats() { return protocol_.stats(); }
    auto& gw_stats() { return stats_; } 

    // dispatch parameters
    void on_parameters_updated(const core::Parameters& params) {
        params["pcaps"].copy(inputs_);

        auto streams_p = params["connections"];
        protocol_.on_parameters_updated(streams_p);
        
        filter(params["filter"]); // FIXME: get filter from streams automatically
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

    void run() {
        protocol_.start();
        for(auto& input: inputs_) {
            TOOLBOX_INFO<<"pcap replay started: "<<input;
            device_.input(input);
            device_.run();
            TOOLBOX_INFO<<"pcap replay finished: "<<input;
        }
        stop();
    }
    void report(std::ostream& os) {
        //protocol_.stats().report(os);
        stats_.report(os);
    }
    void on_idle() {
        report(std::cerr);
    }
    core::TickStream& ticks(core::StreamName stream) {
        return protocol_.ticks(stream);
    }
    core::InstrumentStream& instruments(core::StreamName stream) {
        return protocol_.instruments(stream);
    }

private:
    void on_packet_(const tb::PcapPacket& pkt) {
        switch(pkt.header().protocol().protocol()) {
            case IPPROTO_TCP: case IPPROTO_UDP: {
                stats_.on_received(pkt);
                if(filter_(pkt.header())) {
                    protocol_.on_packet(pkt);
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
    toolbox::PcapDevice device_;
    Stats stats_;
    toolbox::EndpointsFilter filter_;
    std::vector<std::string> inputs_;
};

} // ft::pcap