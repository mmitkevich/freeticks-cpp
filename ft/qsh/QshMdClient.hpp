#pragma once
#include "ft/core/Component.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/StreamStats.hpp"
#include "ft/utils/Common.hpp"
#include "QshDecoder.hpp"
#include "ft/io/Service.hpp"
#include "ft/core/MdClient.hpp"
#include <ostream>
#include <string_view>

namespace ft::qsh {

// file-based gateway for Qsh files
class QshMdClient:  public io::BasicService<QshMdClient, tb::Scheduler, core::State, core::Component>
{
    using Base = io::BasicService<QshMdClient, tb::Scheduler, core::State, core::Component>;
  public:
    using typename Base::Reactor;
  public:
    using Base::Base;
    using Base::state;
    
    void start() { state(core::State::Starting); state(core::State::Started); run(); }
    void stop() {state(core::State::Stopping); state(core::State::Stopped); }

    /// read input file
    void run() {
        std::ifstream ifs(url_);
        decoder_.input(ifs);
        decoder_.run();
    }
    core::StreamStats& stats() { return decoder_.ticks().stats(); }
    
    core::TickStream& ticks(core::StreamTopic topic) { return decoder_.ticks(); }
    core::InstrumentStream& instruments(core::StreamTopic topic) { return decoder_.instruments(); }
private:
    QshDecoder decoder_;
    std::string url_;
};

} // ns