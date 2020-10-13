#pragma once
#include "ft/utils/Common.hpp"
#include "ft/utils/TimeUtils.hpp"
#include "ft/core/Stream.hpp"
#include "toolbox/sys/Time.hpp"
#include "toolbox/util/Slot.hpp"
#include <cstdint>
#include "ft/core/Identifiable.hpp"

namespace ft::core {

using Timestamp = toolbox::WallTime;
using Qty = std::int64_t;
using Price = Qty;
using ClientId = Identifier;
using ExchangeId = Identifier;

struct DefaultPolicy {
    using ExchangeId = ft::core::ExchangeId;
    using ClientId = ft::core::ClientId;
    using Price = ft::core::Price;
    using Qty = ft::core::Qty;
    using Timestamp = ft::core::Timestamp;
    using VenueInstrumentId = ft::core::VenueInstrumentId;
};

enum class TickType : std::uint8_t {
    Unknown = 0,
    Place   = 1,
    Cancel  = 2,
    Move    = 3,
    Fill    = 4
};

enum class TickSide: std::int8_t {
    Invalid = 0,
    Buy     = 1,
    Sell    = -1,
};

static constexpr Price CorePriceMultiplier = 100'000'000;

template<Price VenueMultiplier, Price CoreMultiplier=CorePriceMultiplier>
struct PriceScale {
    core::Price to_core(Price venue_price) { return venue_price * CoreMultiplier / VenueMultiplier; }
    core::Price to_venue(Price core_price) { return core_price * VenueMultiplier / CoreMultiplier; }
};

template<typename PolicyT>
struct BasicTickHeader {
    using ClientId = typename PolicyT::ClientId;
    using ExchangeId = typename PolicyT::ExchangeId;
    using Timestamp = typename PolicyT::Timestamp;

    std::uint64_t flags {};
    ClientId id {};
    ExchangeId server_id {};
    Timestamp timestamp {};         // client
    Timestamp server_timestamp {};  // exchange
};

template<typename PolicyT>
struct BasicTick : BasicTickHeader<PolicyT>{
public:
    using Base = BasicTickHeader<PolicyT>;
    using Price = typename PolicyT::Price;
    using Qty = typename PolicyT::Qty;
    using ClientId = typename Base::ClientId;
    using ExchangeId = typename Base::ExchangeId;
    using Timestamp = typename Base::Timestamp;
public:    
    TickType type() const {
        using namespace tbu::operators;
        if(flags & TickType::Place)
            return TickType::Place;
        else if(flags & TickType::Cancel)
            return TickType::Cancel;
        else if(flags & TickType::Fill)
            return TickType:: Fill;
        assert(false);
        return TickType::Unknown;
    }
public:
    using Base::flags;
    using Base::id;
    using Base::server_id;

    VenueInstrumentId instrument_id;    
    TickSide side {};
    Price price {};
    Qty qty {};
    Qty qty_active {};
    Qty fill_id {};
    Price fill_price {};
    Qty open_interest {};
};

using Tick = BasicTick<DefaultPolicy>;
using TickStream = core::Stream<Tick>;
using TickSignal = tbu::Signal<Tick>;
using TickSlot = TickSignal::Slot;

template<typename PolicyT>
inline std::ostream & operator<<(std::ostream& os, const BasicTick<PolicyT> &e) {
    os<<"type:";
    switch(e.type()) {
        case TickType::Place: os << "Place"; break;
        case TickType::Cancel: os << "Cancel"; break;
        case TickType::Fill: os << "Fill"; break;
        default: assert(false);
    }
    os << ",time:";
    os << e.timestamp;
    os << ",price:";
    os << e.price;
    os << ",qty:";
    os << e.qty;
    return os;
}

} // namespace