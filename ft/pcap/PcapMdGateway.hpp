#pragma once
#include "ft/core/Executor.hpp"
#include "ft/core/Instrument.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/StreamStats.hpp"
#include "ft/utils/Common.hpp"
#include "ft/utils/StringUtils.hpp"
#include "ft/core/MdGateway.hpp"

#include "toolbox/net/Endpoint.hpp"
#include "toolbox/net/EndpointFilter.hpp"
#include "toolbox/net/Pcap.hpp"
#include "toolbox/sys/Log.hpp"
#include "toolbox/sys/Time.hpp"
//#include <netinet/in.h>

#include "ft/core/EndpointStats.hpp"

namespace ft::pcap {



template<typename ProtocolT, typename ExecutorT=core::Executor>
class PcapMdGateway : public core::BasicMdGateway<PcapMdGateway<ProtocolT, ExecutorT>, ExecutorT> {
public:
    using This = PcapMdGateway<ProtocolT, ExecutorT>;
    using Base = core::BasicMdGateway<This, ExecutorT>;
    using Protocol = ProtocolT;
    using Stats = core::EndpointStats<tb::IpEndpoint>;
    using BinaryPacket = tb::PcapPacket;
    using Base::stop;
public:
    template<typename...ArgsT>
    PcapMdGateway(tb::Reactor* reactor, ArgsT...args)
    : Base(reactor)
    , protocol_(*this, std::forward<ArgsT>(args)...)
    {
        device_.packets().connect(tbu::bind<&PcapMdGateway::on_packet_>(this));
    }


    Protocol& protocol() {   return protocol_; }
    toolbox::PcapDevice& device() { return device_; }
    
    Stats& stats() { return stats_; }

    // dispatch parameters
    void on_parameters_updated(const core::Parameters& params) {
        params["pcaps"].copy(inputs_);

        auto streams_p = params["streams"];
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
        for(auto& input: inputs_) {
            TOOLBOX_INFO<<"pcap replay started: "<<input;
            device_.input(input);
            device_.run();
            TOOLBOX_INFO<<"pcap replay finished: "<<input;
        }
        stop();
    }
    void report(std::ostream& os) {
        protocol_.stats().report(os);
        stats_.report(os);
    }
    void on_idle() {
        report(std::cerr);
    }
    core::TickStream& ticks(core::StreamType streamtype) {
        return protocol_.ticks(streamtype);
    }
    core::VenueInstrumentStream& instruments(core::StreamType streamtype) {
        return protocol_.instruments(streamtype);
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