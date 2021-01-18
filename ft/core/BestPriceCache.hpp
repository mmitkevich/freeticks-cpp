#pragma once

#include "ft/core/Identifiable.hpp"
#include "ft/core/InstrumentsCache.hpp"
#include "ft/core/MdClient.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/Tick.hpp"
#include "ft/utils/Common.hpp"
#include "ft/core/Instrument.hpp"
#include "toolbox/sys/Time.hpp"
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <cstdint>
#include <string_view>
#include "ft/core/BestPrice.hpp"

namespace ft { inline namespace core {

template<typename PolicyT>
class BasicBestPriceCache {
public:
    using Policy = PolicyT;
    using BestPrice = BasicBestPrice<Policy>;

    BestPrice& operator[](VenueInstrumentId id) { 
        return data_[id]; 
    }

    template<class TickT>
    auto& update(const TickT& tick) { 
        auto id = tick.venue_instrument_id();
        auto& bp = data_[id];
        bp.venue_instrument_id(id);
        bp.send_time(tick.send_time());
        bp.recv_time(tick.recv_time());
        for(int i=0; i<tick.size(); i++) {
            bp.update(tick[i]);
        }
        return bp;
    }
private:
    ft::unordered_map<VenueInstrumentId, BestPrice> data_;
};
using BestPriceCache = BasicBestPriceCache<typename Tick::Element::Policy>;

}} // ft::core