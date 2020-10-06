#pragma once
#include "ft/utils/Common.hpp"
#include "ft/utils/StringUtils.hpp"
#include "toolbox/net/Pcap.hpp"

namespace ft::pcap {

template<typename ProtocolT>
class PcapMdGateway {
public:
    using Protocol = ProtocolT;
public:
    PcapMdGateway(Protocol&& protocol)
    : protocol_(protocol)
    {}
    PcapMdGateway(const PcapMdGateway& rhs) = delete;
    PcapMdGateway(PcapMdGateway&& rhs) = delete;

    Protocol& protocol() {   return protocol_; }
    void on_packet(const tb::PcapPacket& packet)
    {
        // FIXME: dst_address instead of dst_host
        if((!dst_host_ ||  packet.dst_host() == dst_host_.value())
                && (!dst_port_ || packet.dst_port() == dst_port_.value())) {
            total_count_++;
            TOOLBOX_DEBUG << packet << " | " << ftu::to_hex({packet.data(), packet.size()});
            protocol_.decode({packet.data(), packet.size()});
        }
    }
    template<typename FilterT>
    void filter(const FilterT& flt)
    {
        dst_host(flt.dst_host);
        dst_port(flt.dst_port);
    }
    void dst_host(std::optional<std::string_view> dst_host) 
    {
        dst_host_ = dst_host;
    }
    void dst_port(std::optional<uint64_t> dst_port)
    {
        dst_port_ = dst_port;
    }
    void input(std::string_view input) 
    {
        device_.input(input);
    }
    std::string_view input()
    {
        return device_.input();
    }
    void run()
    {
        device_.packet(tbu::bind<&PcapMdGateway::on_packet>(this));
        device_.run();
    }
    std::size_t total_count() const { return total_count_; }
private:
    Protocol protocol_;
    toolbox::PcapDevice device_;
    std::size_t total_count_{0};
    std::optional<std::string> dst_host_;
    std::optional<std::uint32_t> dst_port_;
};

} // ft::md