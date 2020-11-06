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
    L1      = 13,
    L2      = 14,
    L3      = 15
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
        case TickType::L1: return os << "L1";
        case TickType::L2: return os << "L1";
        case TickType::L3: return os << "L1";
        default: return os << (int)tb::unbox(self);
    }
}

typedef std::int8_t ti_side_t;

enum class TickSide: ti_side_t {
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
using PriceConversion = ft::utils::NumericConversion<ValueT, Multiplier, Price, CorePriceMultiplier>;

template<typename ValueT, std::int64_t Multiplier>
using QtyConversion = ft::utils::NumericConversion<ValueT, Multiplier, Qty, CoreQtyMultiplier>;

using Sequence = std::uint64_t;

// stable binary data format
typedef  std::int64_t ti_flags_t;
struct tick_price_t {
    struct {
        ti_flags_t ti_type:8;
        ti_flags_t ti_side:2;        
    } ti_flags {};
    ClientId ti_client_id {};               // client order or fill id
    ExchangeId ti_server_id {};             // server order or fill id
    Timestamp ti_timestamp {};              // exchange timestamp    
    Qty ti_qty {};                          // original/active qty
    Price ti_price {};
};

struct tick_version_t {
    uint16_t ti_template;
    uint16_t ti_schema;
    uint16_t ti_version;
};
struct tick_t {
    uint16_t ti_len;                    // length of message(without repeating group)
    tick_version_t ti_template;         // template (3*16bit) SBE compatible
    struct {                            // 64bit flags
        ti_flags_t ti_type:8;
    } ti_flags {};
    
    Sequence ti_sequence {};            // sender sequence number    
    Timestamp ti_recv_time {};          // receiver timestamp
    Timestamp ti_send_time {};          // sender timestamp

    VenueInstrumentId ti_venue_instrument_id;   // instrument symbol hash (64 bit)

    std::uint16_t ti_item_len{};       // SBE: blockLength
    std::uint16_t ti_items_count{};    // SBE: numInGroup
    char ti_items[0];                  // data block, SBE: "repeating group"
};

template<typename PolicyT, std::size_t FixedLength=sizeof(tick_price_t)>
struct BasicTickPrice : tick_price_t {
public:
    constexpr static std::size_t fixed_length() { return FixedLength; }
    
    TickType type() const { return (TickType)(ti_flags.ti_type); }
    void type(TickType type) { ti_flags.ti_type = tb::unbox(type); }

    // following could be group of 
    TickSide side() const { return (TickSide)ti_flags.ti_side; }
    void side(TickSide side) { ti_flags.ti_side = tb::unbox(side); }
    
    Price price() const { return ti_price; }
    void price(Price val) { ti_price = val; }

    Qty qty() const { return ti_qty; }
    void qty(Qty val) { ti_qty = val; }

    bool empty() { return type()==TickType::Unknown; }

    // order
    ExchangeId server_id() const { return ti_server_id; }   // to be made up to 64 byte len?
    void server_id(ExchangeId val) { ti_server_id = val; }
};

template<typename T, std::size_t SizeI=1>
struct BasicTicks : tick_t {
public:
    std::size_t length() const { return ti_len + ti_items_count*ti_item_len; }
    static constexpr std::size_t capacity() { return SizeI; }
    TickType type() const { return (TickType)(ti_flags.ti_type); }
    void type(TickType type) { ti_flags.ti_type = tb::unbox(type); }

    Sequence sequence() const { return ti_sequence; }
    void sequence(Sequence val) { ti_sequence = val; }

    Timestamp recv_time() const { return ti_recv_time; }
    void recv_time(Timestamp val) { ti_recv_time = val; }

    Timestamp send_time() const { return ti_send_time; }
    void send_time(Timestamp val) { ti_send_time = val; }    

    VenueInstrumentId venue_instrument_id() const { return ti_venue_instrument_id; }
    void venue_instrument_id(VenueInstrumentId val) { ti_venue_instrument_id = val; }

    const T* begin() const { return &operator[](0); }
    const T* end() const { return &operator[](size()-1); }

    bool empty() const { 
        auto t = type();
        return t == TickType::Unknown || size()==0;
    }
    T& operator[](std::size_t i) { 
        return *reinterpret_cast<T*>(&ti_items[0]+ T::fixed_length()*i);
    }
    const T& operator[](std::size_t i) const { 
        return *reinterpret_cast<const T*>(&ti_items[0]+ T::fixed_length()*i);
    }
    std::size_t size() const  {
        return ti_items_count;
    }
    void resize(std::size_t size) {
        assert(size<=capacity());
        ti_items_count = size;
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
using TickStream = core::Stream<Tick>;
using TickSignal = tbu::Signal<Tick>;
using TickSlot = TickSignal::Slot;

template<typename PolicyT, std::size_t FixedLength>
inline std::ostream& operator<<(std::ostream& os, const BasicTickPrice<PolicyT, FixedLength>& e) {
    os << "ty:'" << e.type() << "'";
    os << ",side:"<<e.side();
    os << ",price:" << PolicyT::instance().price_conv().to_double(e.ti_price);
    os << ",qty:" << PolicyT::instance().qty_conv().to_double(e.ti_qty);
    return os;
}
template<typename DataT, std::size_t SizeI>
inline std::ostream & operator<<(std::ostream& os, const BasicTicks<DataT, SizeI> &e) {
    os << "seq:"<<e.ti_sequence;
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

} // namespace