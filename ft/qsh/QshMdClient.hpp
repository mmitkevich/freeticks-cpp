#pragma once
#include "ft/core/Component.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/StreamStats.hpp"
#include "ft/utils/Common.hpp"
#include "QshDecoder.hpp"

#include "ft/core/MdClient.hpp"
#include <ostream>
#include <string_view>

namespace ft::qsh {

// file-based gateway for Qsh files
class QshMdClient: public core::BasicComponent<QshMdClient> {
public:
    using This = QshMdClient;
    using Base = core::BasicComponent<QshMdClient>;
    using Base::Base;
    
    /// read input file
    void run() {
        std::ifstream ifs(url_);
        decoder_.input(ifs);
        decoder_.run();
    }
    core::StreamStats& stats() { return decoder_.ticks().stats(); }
    
    core::TickStream& ticks(core::StreamName stream) { return decoder_.ticks(); }
    core::VenueInstrumentStream& instruments(core::StreamName stream) { return decoder_.instruments(); }
private:
    QshDecoder decoder_;
    std::string url_;
};

} // ns