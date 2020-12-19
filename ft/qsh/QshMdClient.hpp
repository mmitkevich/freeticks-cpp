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
class QshMdClient: public core::BasicComponent<core::State>,
    public core::BasicParameterized<QshMdClient> {
public:
    using This = QshMdClient;
    using Base = core::BasicComponent<core::State>;
    
    template<typename ReactorT>
    QshMdClient(ReactorT* r) {}

    void start() { state(core::State::Starting); state(core::State::Started); run(); }
    void stop() {state(core::State::Stopping); state(core::State::Stopped); }

    /// read input file
    void run() {
        std::ifstream ifs(url_);
        decoder_.input(ifs);
        decoder_.run();
    }
    core::StreamStats& stats() { return decoder_.ticks().stats(); }
    
    core::TickStream& ticks(core::StreamName stream) { return decoder_.ticks(); }
    core::InstrumentStream& instruments(core::StreamName stream) { return decoder_.instruments(); }
private:
    QshDecoder decoder_;
    std::string url_;
};

} // ns