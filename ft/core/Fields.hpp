#pragma once
#include "ft/core/Component.hpp"
#include "ft/core/Identifiable.hpp"
#include "ft/utils/Numeric.hpp"

#include "toolbox/util/Enum.hpp"
#include "toolbox/sys/Time.hpp"
#include <cstdint>
#include <ostream>

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

enum class Fields: uint64_t {
    BidPrice    = 1<<0,
    BidQty      = 1<<1,
    AskPrice    = 1<<2,
    AskQty      = 1<<3,
    Time        = 1<<4,
    LocalTime   = 1<<5,
    LastPrice   = 1<<6,
    LastQty     = 1<<7,
    LastTime    = 1<<8,
    VenueInstrumentId  = 1<<16,
    VenueSymbol = 1<<17,
    InstrumentId = 1<<18,
    Exchange    = 1<<19,
    Symbol      = 1<<20
};

inline std::ostream& operator<<(std::ostream& os, Fields self) {
    switch(self) {
        case Fields::BidPrice: return os << "BidPrice";
        case Fields::BidQty: return os << "BidQty";
        case Fields::AskPrice: return os << "AskPrice";
        case Fields::AskQty: return os << "AskQty";
        case Fields::Time: return os << "Time";
        case Fields::LocalTime: return os << "LocalTime";
        case Fields::InstrumentId: return os << "InstrumentId";
        case Fields::Symbol: return os << "Symbol";
        case Fields::LastPrice: return os << "LastPrice";
        case Fields::LastQty: return os << "LastQty";
        case Fields::LastTime: return os << "LastTime";
        case Fields::VenueInstrumentId: return os << "VenueInstrumentId";
        case Fields::Exchange: return os << "Exchange";
        case Fields::VenueSymbol: return os <<"VenueSymbol";
        default: return os << toolbox::unbox(self);
    }
}

template<class Self, typename FieldsT>
class BasicFields {
    FT_MIXIN(Self);
public:
    using Fields = FieldsT;
    uint64_t fields_mask() const { return fields_mask_;}
    uint64_t& fields_mask() { return fields_mask_; }
    bool contains_field(Fields field) const {
        return fields_mask_ & tb::unbox(field);
    }
protected:
    uint64_t fields_mask_{};
};

template<class Self, typename FieldsT>
class BasicFieldsWriter : public BasicFields<Self, FieldsT> {
    using Base = BasicFields<Self, FieldsT>;
    FT_MIXIN(Self);
public:
    using Fields = FieldsT;
    void write_field(std::ostream& os, Fields field) const {
        switch(field) {
            case Fields::InstrumentId: os << self()->instrument_id(); break;
            case Fields::Exchange: os << "\""<<self()->exchange()<<"\""; break;
            case Fields::VenueSymbol: os << "\""<<self()->venue_symbol()<<"\""; break;
            case Fields::Symbol: os << "\""<<self()->symbol()<<"\""; break;
            case Fields::VenueInstrumentId: os << self()->venue_instrument_id(); break;
            case Fields::Time: os << toolbox::sys::put_time<toolbox::Nanos>(self()->send_time()); break;
            case Fields::LocalTime: os << toolbox::sys::put_time<toolbox::Nanos>(self()->recv_time()); break;
            default:  os << "<invalid>";
        }
    }
    InstrumentId instrument_id() const { return {}; }
    VenueInstrumentId venue_instrument_id() const { return {}; }
    std::string_view venue_symbol() const { return {}; }    
    std::string_view exchange() const { return {}; }
    std::string_view symbol() const { return {}; }
    ft::Timestamp send_time() const { return {}; }
    ft::Timestamp recv_time() const { return {}; }
};

}} // ft::core