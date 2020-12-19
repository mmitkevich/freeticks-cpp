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

#include "ft/capi/ft-types.h"
#include "ft/core/Requests.hpp"

namespace ft { inline namespace core {

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
    using PriceConv = ft::DoubleConversion<std::int64_t>;
    using QtyConv = ft::DoubleConversion<std::int64_t>;
    
    static constexpr DefaultPolicy instance() { return DefaultPolicy(); }
    constexpr PriceConv price_conv() { return PriceConv(CorePriceMultiplier); }
    constexpr QtyConv qty_conv() { return QtyConv(CoreQtyMultiplier); }
};

enum class TickType : ft_byte {
    Invalid = 0,
    Add     = FT_ADD,         // L3: insert new order 
    Remove  = FT_REMOVE,        // L3: cancel order
    Fill    = FT_EXEC_FILL,                    // L3: order fill
    PartFill= FT_EXEC_PARTIAL_FILL,    // L3: order fill
    Update  = FT_UPDATE,                // L2: update aggregated volume on level
    Clear   = FT_CLEAR,                  // L2: clear book (side)
    Tick    = FT_TICK           // any of above
};

inline std::ostream& operator<<(std::ostream& os, const TickType self) {
    switch(self) {
        case TickType::Invalid: return os << "Unknown";
        case TickType::Add: return os << "Place";
        case TickType::Remove: return os << "Cancel";
        case TickType::Fill: return os << "Fill";
        case TickType::PartFill: return os << "PartFill";
        case TickType::Update: return os <<"Update";
        case TickType::Clear: return os <<"Clear";
        default: return os << (int)tb::unbox(self);
    }
}



enum class TickSide: ft_side_t {
    Invalid = 0,
    Buy     = 1,
    Sell    = -1,
};

inline std::ostream& operator<<(std::ostream& os, const TickSide self) {
    switch(self) {
        case TickSide::Invalid: return os<<"Invalid";
        case TickSide::Buy: return os<<"Buy";
        case TickSide::Sell: return os<<"Sell";
        default: return os<<(int)tb::unbox(self);
    }
}
template<typename ValueT, std::int64_t Multiplier>
using PriceConversion = ft::NumericConversion<ValueT, Multiplier, Price, CorePriceMultiplier>;

template<typename ValueT, std::int64_t Multiplier>
using QtyConversion = ft::NumericConversion<ValueT, Multiplier, Qty, CoreQtyMultiplier>;

using Sequence = std::uint64_t;



template<typename PolicyT, std::size_t FixedLength=sizeof(ft_price_t)>
struct BasicTickPrice : ft_price_t {
public:
    constexpr static std::size_t fixed_length() { return FixedLength; }
    
    TickType type() const { return (TickType)(ft_type); }
    void type(TickType type) { ft_type = tb::unbox(type); }

    // following could be group of 
    TickSide side() const { return (TickSide)ft_side; }
    void side(TickSide side) { ft_side = tb::unbox(side); }
    
    Price price() const { return ft_price; }
    void price(Price val) { ft_price = val; }

    Qty qty() const { return ft_qty; }
    void qty(Qty val) { ft_qty = val; }

    bool empty() { return type()==TickType::Invalid; }

    // order
    ExchangeId server_id() const { return ft_server_id; }   // to be made up to 64 byte len?
    void server_id(ExchangeId val) { ft_server_id = val; }
};


template<typename T, std::size_t SizeI=1>
struct BasicTicks : ft_tick_t {
public:
    std::size_t length() const { return ft_hdr.ft_len + ft_items_count*ft_item_len; }
    static constexpr std::size_t capacity() { return SizeI; }
    
    TickType type() const { return (TickType)ft_hdr.ft_type.ft_event; }
    void type(TickType type) { ft_hdr.ft_type.ft_event = tb::unbox(type); }
    
    StreamType topic() const { return (StreamType)ft_hdr.ft_type.ft_topic; }
    void topic(StreamType topic) { ft_hdr.ft_type.ft_topic = topic; }

    Sequence sequence() const { return ft_hdr.ft_seq; }
    void sequence(Sequence val) { ft_hdr.ft_seq = val; }

    Timestamp recv_time() const { return Timestamp(tb::Nanos(ft_hdr.ft_recv_time)); }
    void recv_time(Timestamp val) { ft_hdr.ft_recv_time = val.time_since_epoch().count(); }

    Timestamp send_time() const { return Timestamp(tb::Nanos(ft_hdr.ft_send_time)); }
    void send_time(Timestamp val) { ft_hdr.ft_send_time = val.time_since_epoch().count(); }    

    InstrumentId instrument_id() const { return ft_instrument_id; }
    void instrument_id(VenueInstrumentId val) { ft_instrument_id = val; }

    VenueInstrumentId venue_instrument_id() const { return ft_venue_instrument_id; }
    void venue_instrument_id(VenueInstrumentId val) { ft_venue_instrument_id = val; }

    const T* begin() const { return &operator[](0); }
    const T* end() const { return &operator[](size()-1); }

    bool empty() const { 
        auto t = type();
        return t == TickType::Invalid || size()==0;
    }
    T& operator[](std::size_t i) { 
        return *reinterpret_cast<T*>(&ft_items[0]+ T::fixed_length()*i);
    }
    const T& operator[](std::size_t i) const { 
        return *reinterpret_cast<const T*>(&ft_items[0]+ T::fixed_length()*i);
    }
    std::size_t size() const  {
        return ft_items_count;
    }
    void resize(std::size_t size) {
        assert(size<=capacity());
        ft_items_count = size;
    }
    template<std::size_t NewSizeI>
    BasicTicks<T,NewSizeI>& as_size() {
        return *reinterpret_cast<BasicTicks<T, NewSizeI>*>(this);
    }
    template<std::size_t NewSizeI>
    const BasicTicks<T,NewSizeI>& as_size() const {
        return *reinterpret_cast<const BasicTicks<T, NewSizeI>*>(this);
    }
private:
    T data_[SizeI];
};


template<std::size_t SizeI>
using Ticks = BasicTicks<BasicTickPrice<DefaultPolicy>, SizeI>;
using Tick = Ticks<1>;
using TickStream = core::Stream<const Tick&>;
using TickSink = core::Sink<const Tick&>;

template<typename PolicyT, std::size_t FixedLength>
inline std::ostream& operator<<(std::ostream& os, const BasicTickPrice<PolicyT, FixedLength>& e) {
    os << "ty:'" << e.type() << "'";
    os << ",side:"<<e.side();
    os << ",price:" << PolicyT::instance().price_conv().to_double(e.ft_price);
    os << ",qty:" << PolicyT::instance().qty_conv().to_double(e.ft_qty);
    return os;
}
template<typename DataT, std::size_t SizeI>
inline std::ostream & operator<<(std::ostream& os, const BasicTicks<DataT, SizeI> &e) {
    os << "seq:"<<e.sequence();
    os << ",sts:'" << toolbox::sys::put_time<toolbox::Nanos>(e.send_time())<<"'";
    os << ",clt:" << (e.recv_time()-e.send_time()).count();
    os << ",vins:" << e.venue_instrument_id();
    os << ",ty:'" << e.type() << "'";
    os << ",d:[";
    for(std::size_t i=0;i<e.size(); i++) {
        if(i>0)
            os << ",";
        os << "{"<<e[i]<<"}";
    }
    os <<"]";
    return os;
}

}} // namespace