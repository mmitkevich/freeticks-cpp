#pragma once
#include "ft/utils/Common.hpp"
#include "QshDecoder.hpp"

#include "ft/core/MdGateway.hpp"

namespace ft::qsh {

// file-based gateway for Qsh files
class QshMdGateway: public core::MdGateway {
public:
    using This = QshMdGateway;
    /// setup filter
    template<typename FilterT>
    void filter(const FilterT &flt) {}
    /// read input file
    void run() override {
        std::ifstream input_stream(input_);
        QshDecoder decoder(input_stream, stats());
        decoder.signals().connect(tbu::bind<&This::on_tick>(this));
        decoder.run();
    }
    void on_tick(const Tick& tick) {
           
    }
};

} // ns