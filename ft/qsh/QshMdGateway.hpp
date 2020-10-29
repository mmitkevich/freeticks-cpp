#pragma once
#include "ft/core/Parameters.hpp"
#include "ft/core/StreamStats.hpp"
#include "ft/utils/Common.hpp"
#include "QshDecoder.hpp"

#include "ft/core/MdGateway.hpp"
#include <ostream>
#include <string_view>

namespace ft::qsh {

// file-based gateway for Qsh files
class QshMdGateway: public core::BasicMdGateway<QshMdGateway> {
public:
    using This = QshMdGateway;
    QshMdGateway() 
    {}

    void url(std::string_view val) { url_ = val; }
    std::string_view url() { return url_; }
    
    /// read input file
    void run() {
        std::ifstream ifs(url_);
        decoder_.input(ifs);
        decoder_.run();
    }
    core::StreamStats& stats() { return decoder_.ticks().stats(); }
    
    core::TickStream& ticks(core::StreamType streamtype) { return decoder_.ticks(); }
    core::VenueInstrumentStream& instruments(core::StreamType streamtype) { return decoder_.instruments(); }
private:
    void on_tick(const Tick& tick) {
           
    }
private:
    QshDecoder decoder_;
    std::string url_;
};

} // ns