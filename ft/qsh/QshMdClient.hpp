#pragma once
#include "ft/core/Component.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/StreamStats.hpp"
#include "ft/utils/Common.hpp"
#include "QshDecoder.hpp"
#include "ft/io/Service.hpp"
#include "ft/core/Client.hpp"
#include <ostream>
#include <string_view>

namespace ft::qsh {

// file-based gateway for Qsh files
class QshMdClient:  public io::BasicService<QshMdClient, io::Service>, public io::BasicSignalSlot<QshMdClient>
{
    using Base = io::BasicService<QshMdClient, io::Service>;
  public:
    using typename Base::Reactor;
  public:
    using Base::Base;
    using Base::state;
    using Base::open, Base::close;

    void do_open() { run(); }

    /// read input file
    void run() {
        std::ifstream ifs(url_);
        decoder_.input(ifs);
        decoder_.run();
    }
    core::StreamStats& stats() { return decoder_.ticks().stats(); }
    
    core::Stream& signal(core::StreamTopic topic) { return decoder_.stream(topic); }
private:
    QshDecoder decoder_;
    std::string url_;
};

} // ns