#pragma once
#include "ft/utils/Common.hpp"

#include "toolbox/net/Endpoint.hpp"
#include "toolbox/net/Protocol.hpp"
#include "toolbox/net/EndpointFilter.hpp"
#include "toolbox/net/McastSock.hpp"
#include "toolbox/sys/Time.hpp"

#include "ft/core/MdGateway.hpp"
#include "ft/core/EndpointStats.hpp"

namespace ft::mcast {

template<typename ProtocolT>
class McastMdGateway : public core::BasicMdGateway<McastMdGateway<ProtocolT>> {
public:
    using Base = core::BasicMdGateway<McastMdGateway<ProtocolT>>;
    using Protocol = ProtocolT;
    using Stats = core::EndpointStats;
    static constexpr toolbox::UdpProtocol transport_protocol = toolbox::UdpProtocol::v4();
public:
    template<typename...ArgsT>
    McastMdGateway(ArgsT...args)
    : protocol_(std::forward<ArgsT>(args)...)
    , sock_(transport_protocol)
    {}

    Protocol& protocol() {   return protocol_; }
    toolbox::net::McastSock& socket() { return sock_; }
    
    Stats& stats() { return stats_; }

    // dispatch parameters
    void on_parameters_updated(const core::Parameters& params) {
    }

    void url(std::string_view url) {
        url_ = url;
        endpoint_ = tbn::parse_ip_endpoint<tbn::UdpProtocol>(url_);
    }
    
    std::string_view url() const { return url_; }

    void start() {
        sock_.join_group(endpoint_.address(), "");
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
    tbn::UdpEndpoint endpoint_;
    std::string url_;
    Protocol protocol_;
    toolbox::net::McastSock sock_;
    Stats stats_;
    toolbox::EndpointsFilter filter_;
};

} // ft::mcasst