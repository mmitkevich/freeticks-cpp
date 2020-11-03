#pragma once
#include "ft/utils/Common.hpp"
#include "ft/utils/Throttled.hpp"
#include <ostream>

namespace ft::core {

class StreamStats {
public:
    template<typename...ArgsT>
    void on_received(ArgsT...args) { received_++; }
    
    //template<typename...ArgsT>
    //void on_accepted(ArgsT...args) { accepted_++; }    

    template<typename...ArgsT>
    void on_rejected(ArgsT...args) { rejected_++; }    

    void on_gap(std::size_t gap_size=1) { gaps_+=gap_size; }
    
    std::size_t received() const { return received_; }
    std::size_t accepted() const { return accepted_; }
    std::size_t rejected() const { return rejected_; }
    std::size_t gaps() const { return gaps_; }

    friend std::ostream& operator<<(std::ostream& os, const StreamStats& self) {
        os << "received:"<<self.received();
        if(self.rejected()>0)
            os<<",rejected:"<<self.rejected();
        if(self.gaps()>0)
            os<<",gaps:"<<self.gaps();
        return os;
    }
protected:
    std::size_t received_{0};
    std::size_t accepted_{0};
    std::size_t rejected_{0}; // rejected on bad format
    std::size_t gaps_{};
};


inline constexpr bool ft_stats_enabled() {
    return false;
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
    template<typename T>
    void on_received(const T& packet) { 
        if constexpr(DerivedT::enabled())
            received_++;
    }
    /*template<typename T>
    void on_accepted(const T& packet) { 
        if constexpr(DerivedT::enabled())
            accepted_++;
    }*/
    void on_idle() {
        report(std::cerr);
    }
protected:
    ft::utils::Throttled<toolbox::Slot<std::ostream&>> report_{ toolbox::bind<&DerivedT::on_report>(static_cast<DerivedT*>(this)) };
};


} // namespace ft::core