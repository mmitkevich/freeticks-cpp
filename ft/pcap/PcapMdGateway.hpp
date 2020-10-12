#pragma once
#include "ft/core/StreamStats.hpp"
#include "ft/utils/Common.hpp"
#include "ft/utils/StringUtils.hpp"
#include "ft/core/MdGateway.hpp"

#include "toolbox/net/Endpoint.hpp"
#include "toolbox/net/Pcap.hpp"
#include "toolbox/sys/Time.hpp"

namespace ft::pcap {

template<typename FnT>
class Throttled {
public:
    Throttled(FnT fn)
    : fn_(std::forward<FnT>(fn))
    {}
    void set_interval(toolbox::Duration val) {
        duration_ = val;
    }
    template<typename ...ArgsT>
    void operator()(ArgsT&&...args) {
        auto now = toolbox::MonoClock::now();
        bool is_time = now - last_time_ > duration_;
        if(is_time) {
            fn_(args...);
            last_time_ = now;            
        }
    }
protected:
    FnT fn_;
    toolbox::MonoTime last_time_ {};
    toolbox::Duration duration_ {};
};

inline constexpr bool ft_stats_enabled() {
    return true;
};

template<typename DerivedT>
class BasicStats : public core::StreamStats {
public:
    static constexpr bool enabled() { return true; }

    BasicStats() {
        auto dur = 
        #ifdef TOOLBOX_DEBUG        
            toolbox::Seconds(10);
        #else
            toolbox::Hours(1);
        #endif
        report_.set_interval(dur);
    }

    void report(std::ostream& os) {
        if constexpr(DerivedT::enabled())
            report_(os);
    }
    template<typename ...ArgsT>
    void on_received(ArgsT...args) { 
        if constexpr(DerivedT::enabled())
            total_received++;
    }
    template<typename ...ArgsT>
    void on_accepted(ArgsT...args) { 
        if constexpr(DerivedT::enabled())
            total_accepted++;
    }
    void on_idle() {
        report(std::cerr);
    }
protected:
    Throttled<toolbox::Slot<std::ostream&>> report_{ toolbox::bind<&DerivedT::on_report>(static_cast<DerivedT*>(this)) };
};

class PcapStats: public BasicStats<PcapStats> {
public:
    using Base = BasicStats<PcapStats>;
    using DstStat = utils::FlatMap<tbn::IpEndpoint, std::size_t>;
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
    void on_accepted(const tb::PcapPacket& pkt) { 
        if constexpr(enabled()) {
            Base::on_accepted();
            const auto& dst = pkt.dst();
            auto current = dst_stat_[dst];
            dst_stat_[dst] = current + 1;
        }
    }    
protected:
    DstStat dst_stat_;
};

template<typename ProtocolT, typename FilterT=toolbox::EndpointsFilter, typename StatsT = PcapStats>
class PcapMdGateway : public core::BasicMdGateway<PcapMdGateway<ProtocolT, FilterT, StatsT>> {
public:
    using Base = core::BasicMdGateway<PcapMdGateway<ProtocolT, FilterT, StatsT>>;
    using Protocol = ProtocolT;
    using Stats = StatsT;
public:
    template<typename...ArgsT>
    PcapMdGateway(ArgsT...args)
    : protocol_(std::forward<ArgsT>(args)...)
    {}
    PcapMdGateway(const PcapMdGateway& rhs) = delete;
    PcapMdGateway(PcapMdGateway&& rhs) = delete;

    Protocol& protocol() {   return protocol_; }
    toolbox::PcapDevice& device() { return device_; }
    
    Stats& stats() { return stats_; }

    void filter(const FilterT& filter) {
        filter_ = filter;
        TOOLBOX_INFO << "PcapMdGateway::filter("<<filter_<<")";
    }
    void url(std::string_view url) {
        Base::url(url);
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
private:
    void on_packet_(const tb::PcapPacket& pkt) {
        switch(pkt.protocol().protocol()) {
            case IPPROTO_TCP: case IPPROTO_UDP: {
                stats_.on_received(pkt);
                if(filter_(pkt)) {
                    stats_.on_accepted(pkt);
                    protocol_.on_packet(pkt);
                    on_idle();      
                }
        } break;
            default: break; // ignore
        }
    }
private:
    Protocol protocol_;
    toolbox::PcapDevice device_;
    Stats stats_;
    FilterT filter_;
};

} // ft::md