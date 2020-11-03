#pragma once
#include "ft/utils/Common.hpp"
#include "ft/utils/TimeUtils.hpp"
#include "ft/core/Stream.hpp"
#include "toolbox/sys/Time.hpp"
#include "toolbox/util/Slot.hpp"
#include <cstdint>
#include <ostream>
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
    //using ExchangeId = ft::core::ExchangeId;
    //using ClientId = ft::core::ClientId;
    //using Price = ft::core::Price;
    //using Qty = ft::core::Qty;
    //using Timestamp = ft::core::Timestamp;
    //using VenueInstrumentId = ft::core::VenueInstrumentId;
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

inline std::ostream& operator<<(std::ostream& os, const TickType self) {
    switch(self) {
        case TickType::Unknown: return os << "Unknown";
        case TickType::Place: return os << "Place";
        case TickType::Cancel: return os << "Cancel";
        case TickType::Modify: return os << "Modify";
        case TickType::Fill: return os << "Fill";
        case TickType::Update: return os <<"Update";
        case TickType::Clear: return os <<"Clear";
        default: return os << tb::unbox(self);
    }
}

enum class TickSide: std::int8_t {
    Invalid = 0,
    Buy     = 1,
    Sell    = -1,
};

inline std::ostream& operator<<(std::ostream& os, const TickSide self) {
    switch(self) {
        case TickSide::Invalid: return os<<"Invalid";
        case TickSide::Buy: return os<<"Buy";
        case TickSide::Sell: return os<<"Sell";
        default: return os<<tb::unbox(self);
    }
}
template<typename ValueT, std::int64_t Multiplier>
using PriceConversion = ft::utils::NumericConversion<ValueT, Multiplier, Price, CorePriceMultiplier>;

template<typename ValueT, std::int64_t Multiplier>
using QtyConversion = ft::utils::NumericConversion<ValueT, Multiplier, Qty, CoreQtyMultiplier>;

using Sequence = std::uint64_t;

//template<typename PolicyT>
struct TickData {
    //using ClientId = typename PolicyT::ClientId;
    //using ExchangeId = typename PolicyT::ExchangeId;
    //using Timestamp = typename PolicyT::Timestamp;

    struct {
        std::int64_t ti_type:4;
        std::int64_t ti_side:2;
    } ti_flags {};

    ClientId ti_id {};
    ExchangeId ti_server_id {};
    Timestamp ti_timestamp {};         // client
    Timestamp ti_server_timestamp {};  // exchange
    Sequence ti_sequence {};
    VenueInstrumentId ti_venue_instrument_id;    
    Price ti_price {};
    Qty ti_qty {};
    Qty ti_qty_active {};
    ExchangeId ti_fill_id {};
    Price ti_fill_price {};
    Qty ti_open_interest {};    
};

template<typename PolicyT>
struct BasicTick : TickData {
public:
    //using Base = BasicTickHeader<PolicyT>;
    //using Price = typename PolicyT::Price;
    //using Qty = typename PolicyT::Qty;
    //using ClientId = typename Base::ClientId;
    //using ExchangeId = typename Base::ExchangeId;
    //using Timestamp = typename Base::Timestamp;
public:
    TickType type() const { return (TickType)(ti_flags.ti_type); }
    void type(TickType type) { ti_flags.ti_type = tb::unbox(type); }
    TickSide side() const { return (TickSide)ti_flags.ti_side; }
    void side(TickSide side) { ti_flags.ti_side = tb::unbox(side); }
    Timestamp timestamp() const { return ti_timestamp; }
    void timestamp(Timestamp val) { ti_timestamp = val; }
    Timestamp server_timestamp() const { return ti_server_timestamp; }
    void server_timestamp(Timestamp val) { ti_server_timestamp = val; }    
    ExchangeId server_id() const { return ti_server_id; }
    void server_id(ExchangeId val) { ti_server_id = val; }
    ExchangeId fill_id() const { return ti_fill_id; }
    void fill_id(ExchangeId val) { ti_fill_id = val; }
    Price price() const { return ti_price; }
    void price(Price val) { ti_price = val; }
    Price fill_price() const { return ti_fill_price; }
    void fill_price(Price val) { ti_fill_price = val; }
    Qty qty() const { return ti_qty; }
    void qty(Qty val) { ti_qty = val; }
    Qty qty_active() const { return ti_qty_active; }
    void qty_active(Qty val) { ti_qty_active = val; }
    Qty open_interest() const { return ti_open_interest; }
    void open_interest(Qty val) { ti_open_interest = val; }
    VenueInstrumentId venue_instrument_id() const { return ti_venue_instrument_id; }
    void venue_instrument_id(VenueInstrumentId val) { ti_venue_instrument_id = val; }
    Sequence sequence() const { return ti_sequence; }
    void sequence(Sequence val) { ti_sequence = val; }

    bool empty() const { 
        auto t = type();
        return t == TickType::Unknown;
    }
    //using Base::flags;
    //using Base::id;
    //using Base::server_id;
};

using Tick = BasicTick<DefaultPolicy>;
using TickStream = core::Stream<Tick>;
using TickSignal = tbu::Signal<Tick>;
using TickSlot = TickSignal::Slot;


template<typename PolicyT>
inline std::ostream & operator<<(std::ostream& os, const BasicTick<PolicyT> &e) {
    os << "seq:"<<e.ti_sequence;
    os << ",sts:'" << toolbox::sys::put_time<toolbox::Nanos>(e.ti_server_timestamp)<<"'";
    os << ",cts-sts:" << (e.ti_timestamp-e.ti_server_timestamp).count();
    os << ",vi:" << e.ti_venue_instrument_id;
    os << ",t:'";
    os << e.type();
    os << "'";
    os << ",s:"<<e.side();
    os << ",price:" << PolicyT::instance().price_conv().to_double(e.ti_price);
    os << ",qty:" << PolicyT::instance().qty_conv().to_double(e.ti_qty);
    return os;
}

} // namespace