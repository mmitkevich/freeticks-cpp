#pragma once
#include "ft/core/Instrument.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/StreamStats.hpp"
#include "ft/utils/Common.hpp"
#include "ft/utils/StringUtils.hpp"
#include "ft/core/MdGateway.hpp"

#include "toolbox/net/Endpoint.hpp"
#include "toolbox/net/EndpointFilter.hpp"
#include "toolbox/net/Pcap.hpp"
#include "toolbox/sys/Time.hpp"
//#include <netinet/in.h>

#include "ft/core/EndpointStats.hpp"

namespace ft::pcap {



template<typename ProtocolT>
class PcapMdGateway : public core::BasicMdGateway<PcapMdGateway<ProtocolT>> {
public:
    using Base = core::BasicMdGateway<PcapMdGateway<ProtocolT>>;
    using Protocol = ProtocolT;
    using Stats = core::EndpointStats<tb::IpEndpoint>;
public:
    template<typename...ArgsT>
    PcapMdGateway(ArgsT...args)
    : protocol_(*this, std::forward<ArgsT>(args)...)
    {}


    Protocol& protocol() {   return protocol_; }
    toolbox::PcapDevice& device() { return device_; }
    
    Stats& stats() { return stats_; }

    // dispatch parameters
    void on_parameters_updated(const core::Parameters& params) {
        filter(params["filter"]);
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
    void url(std::string_view url) {
        device_.input(url);
    }
    std::string url() const { return device_.input(); }
    void start() { run(); }
    void run() {
        device_.packets().connect(tbu::bind<&PcapMdGateway::on_packet_>(this));
        device_.run();
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
};

} // ft::pcap