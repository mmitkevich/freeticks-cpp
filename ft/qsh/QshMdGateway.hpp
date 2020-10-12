#pragma once
#include "ft/core/StreamStats.hpp"
#include "ft/utils/Common.hpp"
#include "QshDecoder.hpp"

#include "ft/core/MdGateway.hpp"
#include <ostream>

namespace ft::qsh {

// file-based gateway for Qsh files
class QshMdGateway: public core::BasicMdGateway<QshMdGateway> {
public:
    using This = QshMdGateway;
    /// setup filter
    template<typename FilterT>
    void filter(const FilterT &flt) {}
    void start() { run(); }
    /// read input file
    void run() {
        std::ifstream input_stream(url_);
        QshDecoder decoder(input_stream, stats());
        decoder.signals().connect(tbu::bind<&This::on_tick>(this));
        decoder.run();
    }
    core::StreamStats& stats() { return stats_; }
private:
    void on_tick(const Tick& tick) {
           
    }
private:
    core::StreamStats stats_;
};

} // ns