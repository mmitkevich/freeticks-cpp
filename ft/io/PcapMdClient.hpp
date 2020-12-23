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
class PcapMdClient : public core::BasicComponent<core::State>,
    public core::BasicParameterized<PcapMdClient<ProtocolT>> {
public:
    using This = PcapMdClient<ProtocolT>;
    using Base = core::BasicComponent<core::State>;
    using Protocol = ProtocolT;
    using Stats = core::EndpointStats<tb::IpEndpoint>;
    using BinaryPacket = tb::PcapPacket;
public:
    template<typename ReactorT, typename...ArgsT>
    explicit PcapMdClient(ReactorT* r, ArgsT...args)
    : protocol_(std::forward<ArgsT>(args)...)
    {
        device_.packets().connect(tb::bind<&PcapMdClient::on_packet_>(this));
    }
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

    void start() {
        protocol_.open();
        for(auto& input: inputs_) {
            TOOLBOX_INFO<<"pcap replay started: "<<input;
            device_.input(input);
            device_.run();
            TOOLBOX_INFO<<"pcap replay finished: "<<input;
        }
        stop();
    }
    void stop() {
        protocol_.close();
    }
    void report(std::ostream& os) {
        //protocol_.stats().report(os);
        stats_.report(os);
    }
    void on_idle() {
        report(std::cerr);
    }
    core::TickStream& ticks(core::StreamTopic topic) {
        return protocol_.ticks(topic);
    }
    core::InstrumentStream& instruments(core::StreamTopic topic) {
        return protocol_.instruments(topic);
    }

private:
    void on_packet_(const tb::PcapPacket& pkt) {
        switch(pkt.header().protocol().protocol()) {
            case IPPROTO_TCP: case IPPROTO_UDP: {
                stats_.on_received(pkt);
                if(filter_(pkt.header())) {
                    protocol_.on_packet(device_, pkt, {});
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