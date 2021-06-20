#pragma once

#include "ft/core/Identifiable.hpp"
#include "ft/core/Client.hpp"
#include "ft/utils/Common.hpp"
#include "ft/core/Instrument.hpp"
#include "toolbox/util/InternedStrings.hpp"
#include <stdexcept>

namespace ft { inline namespace core {

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
    void update(const core::InstrumentUpdate& val) { 
        auto id = val.venue_instrument_id();
        auto &ins = instruments_[id];
        ins.instrument().symbol(strpool_.intern(val.symbol()));
        ins.exchange(strpool_.intern(val.exchange()));
        ins.instrument_id(val.instrument_id());
    }
    template<typename GatewayT>
    void connect(GatewayT &gw) {
        gw.instruments().connect(tb::bind<&InstrumentsCache::on_instrument>(this));
    }
    template<typename GatewayT>
    void disconnect(GatewayT &gw) {
        gw.instruments().connect(tb::bind<&InstrumentsCache::on_instrument>(this));
    }
    void on_instrument(const core::InstrumentUpdate& vi) {
        update(vi);
    }
    std::string_view symbol(const core::VenueInstrumentId id) {
        if(instruments_.find(id)!=instruments_.end())
            return instruments_[id].symbol();
        else 
            return "";
    }
private:
    ft::unordered_map<VenueInstrumentId, core::VenueInstrument> instruments_;
    tb::InternedStrings strpool_;
};

}} // ft::core