#pragma once
#include "ft/utils/Common.hpp"
#include "ft/utils/TimeUtils.hpp"
#include "ft/core/Stream.hpp"
#include "toolbox/sys/Time.hpp"
#include "toolbox/util/Slot.hpp"
#include <boost/mp11/tuple.hpp>
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

enum class Mod : ft_event_t {
    Empty = 0,
    Add     = FT_ADD,           // L3: insert new order, L2: new level
    Delete  = FT_DEL,        // L3: remove order, L2: remove level
    Modify  = FT_MOD,        // L3: modify order, L2: modify aggr volume on level
    Clear   = FT_CLEAR,         // L2: clear book (side)
};

enum class OrderStatusEvent : ft_event_t {
    New   = FT_ORDER_STATUS_NEW,
    Filled = FT_ORDER_STATUS_FILLED,
    PartFilled = FT_ORDER_STATUS_PART_FILLED,
    Canceled = FT_ORDER_STATUS_CANCELED
};

enum class TickEvent : ft_event_t {
    Empty = 0,
    Add = FT_TICK_ADD,
    Delete = FT_TICK_DEL,
    Modify = FT_TICK_MOD,
    Clear = FT_TICK_CLEAR,
    Fill = FT_TICK_FILL
};

inline std::ostream& operator<<(std::ostream& os, const TickEvent self) {
    switch(self) {
        case TickEvent::Empty: return os << "Empty";
        case TickEvent::Add: return os << "Add";
        case TickEvent::Modify: return os << "Modify";        
        case TickEvent::Delete: return os << "Delete";
        case TickEvent::Clear: return os << "Clear";
        case TickEvent::Fill: return os << "Fill";
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

using TickSequence = std::uint64_t;



template<typename PolicyT>
struct BasicTickElement : ft_price_t {
public:
    using Event = TickEvent;
    using Side = TickSide;
    using Policy = PolicyT;

    constexpr static std::size_t length() { return sizeof(ft_price_t); }
    
    Event event() const { return Event(ft_event); }
    auto& event(Event val) { ft_event = tb::unbox(val); return *this; }

    // following could be group of 
    TickSide side() const { return TickSide(ft_side); }
    auto& side(TickSide side) { ft_side = tb::unbox(side); return *this; }
    
    Price price() const { return ft_price; }
    auto& price(Price val) { ft_price = val; return *this; }

    Qty qty() const { return ft_qty; }
    auto& qty(Qty val) { ft_qty = val; return *this; }

    bool empty() { return event()==TickEvent::Empty; }

    // order
    ExchangeId server_id() const { return ft_server_id; }   // to be made up to 64 byte len?
    auto& server_id(ExchangeId val) { ft_server_id = val; return *this; }
};

using TickElement = BasicTickElement<TickPolicy>;

template<typename T, std::size_t SizeI=1>
struct BasicTicks : ft_tick_t {
public:
    using Event = core::Event;
    using Topic = core::StreamTopic;
    using Sequence = ft_seq_t;
    using Element = T;
    
    std::size_t bytesize() const { return ft_hdr.ft_len + ft_items_count*ft_item_len; }
    static constexpr std::size_t capacity() { return SizeI; }
    
    Event event() const { return Event(ft_hdr.ft_type.ft_event); }
    void event(Event val) { ft_hdr.ft_type.ft_event = tb::unbox(val); }
    
    Topic topic() const { return Topic(ft_hdr.ft_type.ft_topic); }
    void topic(Topic val) { ft_hdr.ft_type.ft_topic = tb::unbox(val); }

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
        auto ev = event();
        return ev == Event::Empty || size()==0;
    }
    T& operator[](std::size_t i) { 
        return *reinterpret_cast<T*>(&ft_items[0]+ T::length()*i);
    }
    const T& operator[](std::size_t i) const { 
        return *reinterpret_cast<const T*>(&ft_items[0]+ T::length()*i);
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
using Ticks = BasicTicks<TickElement, SizeI>;
using Tick = Ticks<1>;
using TickStream = core::Stream<const Tick&>;
using TickSink = core::Sink<const Tick&>;


template<typename...ArgsT>
constexpr auto make_ticks(ArgsT...args) {
    Ticks<sizeof...(ArgsT)> ti;
    auto tup = std::make_tuple(std::forward<ArgsT>(args)...);
    std::size_t i = 0;
    mp::tuple_for_each(std::move(tup), [&](auto &e) {
        ti[i++] = e;
    });
    return ti;
}

template<typename PolicyT>
inline std::ostream& operator<<(std::ostream& os, const BasicTickElement<PolicyT>& e) {
    os << "e:'" << e.event() << "'";
    os << ", side:'"<<e.side()<<"'";
    os << ", price:" << PolicyT().price_conv().to_double(e.ft_price);
    os << ", qty:" <<PolicyT().qty_conv().to_double(e.ft_qty);
    return os;
}
template<typename DataT, std::size_t SizeI>
inline std::ostream & operator<<(std::ostream& os, const BasicTicks<DataT, SizeI> &e) {
    os << "t: '"<<e.topic()<<"'";
    os << ", e:'" << e.event() << "'";    
    //os << ", s:"<<e.sequence();
    os << ", ts:'" << toolbox::sys::put_time<toolbox::Nanos>(e.send_time())<<"'";
    //os << ", clt:" << (e.recv_time()-e.send_time()).count();
    os << ", viid:" << e.venue_instrument_id();

    os << ", d:[";
    for(std::size_t i=0;i<e.size(); i++) {
        if(i>0)
            os << ",";
        os << "{"<<e[i]<<"}";
    }
    os <<"]";
    return os;
}

}} // namespace