#pragma once
#include "ft/utils/Common.hpp"
#include "ft/core/StreamStats.hpp"

namespace ft::core {

template<typename EndpointT>
class EndpointStats: public BasicStats<EndpointStats<EndpointT>> {
    using Base = BasicStats<EndpointStats<EndpointT>>;
public:
    using Endpoint = EndpointT;    
    using DstStat = utils::FlatMap<Endpoint, std::size_t>;
public:
    using Base::Base;
    using Base::report;
    using Base::on_accepted;
    using Base::on_received;
    static constexpr bool enabled() { return ft_stats_enabled(); }    

public:
    void on_report(std::ostream& os) {
        os << "dst_stat:" << std::endl;
        for(auto& [k,v]: dst_stat_) {
            os <<  std::setw(12) << v << "    " << k << std::endl;
        }
    }
    template<typename PacketT>
    void on_accepted(const PacketT& pkt) { 
        if constexpr(enabled()) {
            Base::on_accepted(pkt);
            const auto& dst = pkt.dst();
            auto current = dst_stat_[dst];
            dst_stat_[dst] = current + 1;
        }
    }    
protected:
    DstStat dst_stat_;
};

}