#pragma once
#include "ft/core/Component.hpp"
#include "ft/core/Identifiable.hpp"
#include "ft/utils/Numeric.hpp"
#include "toolbox/util/Enum.hpp"
#include "toolbox/sys/Time.hpp"
#include <boost/callable_traits/is_invocable.hpp>
#include <cstdint>
#include <ostream>
#include <sstream>
#include <toolbox/util/TypeTraits.hpp>
#include <type_traits>

using namespace boost::callable_traits;

namespace ft { inline namespace core {

using Timestamp = toolbox::WallTime;
using WallClock = toolbox::WallClock;
using Qty = std::int64_t;
using Price = std::int64_t;
using ClientId = Identifier;
using ExchangeId = Identifier;

    
struct PriceQty {
    Price price {0};
    Qty qty {0};
};

struct PriceQtyTime {
    Price price {0};
    Qty qty {0};
    Timestamp time {};
};

static constexpr std::int64_t CorePriceMultiplier = 100'000'000;
static constexpr std::int64_t CoreQtyMultiplier = 1;


struct TickPolicy {
    //using ExchangeId = ft::core::ExchangeId;
    //using ClientId = ft::core::ClientId;
    //using Price = ft::core::Price;
    //using Qty = ft::core::Qty;
    //using Timestamp = ft::core::Timestamp;
    //using VenueInstrumentId = ft::core::VenueInstrumentId;
    using PriceConv = ft::DoubleConversion<std::int64_t>;
    using QtyConv = ft::DoubleConversion<std::int64_t>;
    
    constexpr PriceConv price_conv() { return PriceConv(CorePriceMultiplier); }
    constexpr QtyConv qty_conv() { return QtyConv(CoreQtyMultiplier); }
};


// values represent bits 1..64, 0 is "empty"
enum class Field: uint8_t {
    Empty       = 0,
    // Tick fields
    Seq,
    LocalTime,    
    Time,
    InstrumentId,    
    VenueInstrumentId,
    VenueSymbol,
    Exchange,
    Price,
    Qty,
    Side,
    // Resolved symbol
    Symbol,    
    // BestPrice fields
    BidPrice,
    BidQty,
    AskPrice,
    AskQty,
    LastPrice,
    LastQty,
    LastTime,
    Event,
    Topic
};

inline std::ostream& operator<<(std::ostream& os, Field self) {
    switch(self) {
        case Field::Seq: return os << "Seq";
        case Field::BidPrice: return os << "BidPrice";
        case Field::BidQty: return os << "BidQty";
        case Field::AskPrice: return os << "AskPrice";
        case Field::AskQty: return os << "AskQty";
        case Field::Time: return os << "Time";
        case Field::LocalTime: return os << "LocalTime";
        case Field::InstrumentId: return os << "InstrumentId";
        case Field::Symbol: return os << "Symbol";
        case Field::LastPrice: return os << "LastPrice";
        case Field::LastQty: return os << "LastQty";
        case Field::LastTime: return os << "LastTime";
        case Field::VenueInstrumentId: return os << "VenueInstrumentId";
        case Field::Exchange: return os << "Exchange";
        case Field::VenueSymbol: return os <<"VenueSymbol";
        case Field::Price: return os << "Price";
        case Field::Qty: return os << "Qty";
        case Field::Side: return os << "Side";
        case Field::Event: return os <<"Event";
        case Field::Topic: return os <<"Topic";
        case Field::Empty: return os <<"<empty>";
        default: return os << "F"<<(std::size_t)toolbox::unbox(self);
    }
}

inline std::string to_string(Field field) {
    std::ostringstream ss;
    ss<<field;
    return ss.str();
}


}} // ft::core