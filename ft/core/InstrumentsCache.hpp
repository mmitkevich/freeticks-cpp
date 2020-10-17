#pragma once

#include "ft/core/Identifiable.hpp"
#include "ft/core/MdGateway.hpp"
#include "ft/utils/Common.hpp"
#include "ft/core/Instrument.hpp"
#include "toolbox/util/InternedStrings.hpp"
#include <stdexcept>

namespace ft::core {

class InstrumentError : public std::runtime_error {
public:
    using Base=std::runtime_error;
    using Base::Base;
};

class InstrumentsCache {
public:
    core::VenueInstrument& operator[](VenueInstrumentId id) { 
        return instruments_[id]; 
    }
    void update(const core::VenueInstrument& val) { 
        auto id = val.venue_instrument_id();
        auto &ins = instruments_[id] = val;
        auto sym = symbols_.intern(ins.instrument().symbol());
        ins.instrument().symbol(sym);
    }
    template<typename GatewayT>
    void connect(GatewayT &gw) {
        gw.instruments().connect(tbu::bind<&InstrumentsCache::on_instrument>(this));
    }
    template<typename GatewayT>
    void disconnect(GatewayT &gw) {
        gw.instruments().connect(tbu::bind<&InstrumentsCache::on_instrument>(this));
    }
    void on_instrument(const core::VenueInstrument& vi) {
        update(vi);
    }
private:
    utils::FlatMap<VenueInstrumentId, core::VenueInstrument> instruments_;
    toolbox::util::InternedStrings symbols_;
};
};