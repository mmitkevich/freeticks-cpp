#pragma once
#include "ft/utils/Common.hpp"
#include "ft/utils/TimeUtils.hpp"
#include "ft/core/Stream.hpp"
#include "toolbox/sys/Time.hpp"
#include "toolbox/util/Slot.hpp"
#include <cstdint>
#include "ft/core/Identifiable.hpp"
#include "ft/utils/Numeric.hpp"

namespace ft::core {

using Timestamp = toolbox::WallTime;
using WallClock = toolbox::WallClock;

static constexpr std::int64_t CorePriceMultiplier = 100'000'000;
static constexpr std::int64_t CoreQtyMultiplier = 1;

using Qty = std::int64_t;
using Price = std::int64_t;
using ClientId = Identifier;
using ExchangeId = Identifier;

struct DefaultPolicy {
    using ExchangeId = ft::core::ExchangeId;
    using ClientId = ft::core::ClientId;
    using Price = ft::core::Price;
    using Qty = ft::core::Qty;
    using Timestamp = ft::core::Timestamp;
    using VenueInstrumentId = ft::core::VenueInstrumentId;
    using PriceConv = ft::utils::DoubleConversion<std::int64_t>;
    using QtyConv = ft::utils::DoubleConversion<std::int64_t>;
    
    static constexpr DefaultPolicy instance() { return DefaultPolicy(); }
    constexpr PriceConv price_conv() { return PriceConv(CorePriceMultiplier); }
    constexpr QtyConv qty_conv() { return QtyConv(CoreQtyMultiplier); }
};

enum class TickType : std::uint8_t {
    Unknown = 0,
    Place   = 1, // L3: insert new order 
    Cancel  = 2, // L3: cancel order
    Modify  = 3, // L3: insert new order, with reference to old (cancelled) order
    Fill    = 4, // L3: order fill
    Update  = 5, // L2: update aggregated volume on level
    Clear   = 6, // L2: clear book (side)
};

enum class TickSide: std::int8_t {
    Invalid = 0,
    Buy     = 1,
    Sell    = -1,
};

template<typename ValueT, std::int64_t Multiplier>
using PriceConversion = ft::utils::NumericConversion<ValueT, Multiplier, Price, CorePriceMultiplier>;

template<typename ValueT, std::int64_t Multiplier>
using QtyConversion = ft::utils::NumericConversion<ValueT, Multiplier, Qty, CoreQtyMultiplier>;

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
    void type(TickType type) {
        flags = (flags & ~(0xFF)) | (std::uint8_t) type;
    }
    using Base::flags;
    using Base::id;
    using Base::server_id;

    VenueInstrumentId venue_instrument_id;    
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
    os << "ts:'" << toolbox::sys::put_time(e.timestamp)<<"'";
    os << ",sts:'" << toolbox::sys::put_time(e.server_timestamp)<<"'";
    os << ",vi:" << e.venue_instrument_id;
    os << ",t:'";
    switch(e.type()) {
        case TickType::Place: os << "Place"; break;
        case TickType::Cancel: os << "Cancel"; break;
        case TickType::Fill: os << "Fill"; break;
        default: assert(false);
    }
    os << "'";
    os << ",price:" << PolicyT::instance().price_conv().to_double(e.price);
    os << ",qty:" << PolicyT::instance().qty_conv().to_double(e.qty);
    return os;
}

} // namespace