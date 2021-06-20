#pragma once
#include "ft/utils/Common.hpp"
#include "ft/core/StreamStats.hpp"

namespace ft { inline namespace core {

template<typename EndpointT>
class EndpointStats: public BasicStats<EndpointStats<EndpointT>> {
    using Base = BasicStats<EndpointStats<EndpointT>>;
public:
    using Endpoint = EndpointT;    
    using DstStat = ft::unordered_map<Endpoint, std::size_t>;
public:
    using Base::Base;
    using Base::report;
    //using Base::on_accepted;
    using Base::on_received;
    static constexpr bool enabled() { return ft_stats_enabled(); }    

public:
    void on_report(std::ostream& os) {
        if constexpr(enabled()) {
            for(auto it : dst_stat_) {
                os <<  std::setw(12) << it.second << "    " << it.first << std::endl;
            }
        }
    }
    template<typename PacketT>
    void on_accepted(const PacketT& pkt) { 
        Base::on_accepted(pkt); 
        if constexpr(enabled()) {
            const auto& dst = pkt.header().dst();
            auto current = dst_stat_[dst];
            dst_stat_[dst] = current + 1;
        }
    }    
protected:
    DstStat dst_stat_;
};

}} // ft::core