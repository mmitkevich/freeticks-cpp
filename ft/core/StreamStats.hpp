#pragma once
#include "ft/utils/Common.hpp"
#include "ft/utils/Throttled.hpp"

namespace ft::core {

class StreamStats {
public:
    template<typename...ArgsT>
    void on_received(ArgsT...args) { total_received++; }
    
    template<typename...ArgsT>
    void on_accepted(ArgsT...args) { total_accepted++; }    

    template<typename...ArgsT>
    void on_bad(ArgsT...args) { total_bad++; }    

    std::size_t total_received{0};
    std::size_t total_accepted{0};
    std::size_t total_bad{0}; // rejected on bad format
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
    ft::utils::Throttled<toolbox::Slot<std::ostream&>> report_{ toolbox::bind<&DerivedT::on_report>(static_cast<DerivedT*>(this)) };
};


} // namespace ft::core