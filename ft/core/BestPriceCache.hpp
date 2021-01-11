#pragma once

#include "ft/core/Identifiable.hpp"
#include "ft/core/MdClient.hpp"
#include "ft/core/Tick.hpp"
#include "ft/matching/OrderBook.hpp"
#include "ft/utils/Common.hpp"
#include "ft/core/Instrument.hpp"
#include "toolbox/sys/Time.hpp"
#include "toolbox/util/InternedStrings.hpp"
#include <stdexcept>
#include <cstdint>

namespace ft { inline namespace core {

template<typename PolicyT>
class BasicBestPrice {
public:
    using Policy = PolicyT;
    enum class Fields: uint32_t {
        BidPrice    = 1,
        BidQty      = 2,
        AskPrice    = 4,
        AskQty      = 8,
        SendTime    = 4096,
        RecvTime   = 8192
    };
public:
    VenueInstrumentId venue_instrument_id() const { return venue_instrument_id_; }
    void venue_instrument_id(VenueInstrumentId val) { venue_instrument_id_ = val; }
    bool contains(Fields field) const {
        return flags_ & tb::unbox(field);
    }
    bool empty() const { return flags_==0; }
    void clear() { 
        flags_ = 0;
        ask_ = {};
        bid_ = {};
    }
    void send_time(tb::WallTime time) { 
        send_time_ = time; 
        flags_|=tb::unbox(Fields::SendTime);
    }
    tb::WallTime send_time() const { return send_time_; }
    void recv_time(tb::WallTime time) { 
        recv_time_ = time; 
        flags_|=tb::unbox(Fields::RecvTime);
    }
    tb::WallTime recv_time() const { return recv_time_; }

    auto& bid_price(Price val) { 
        bid_.price = val;
        flags_|=tb::unbox(Fields::BidPrice);
        return *this;
    }
    const Price bid_price() const { return bid_.price; }
    auto& bid_qty(Qty val) {
        bid_.qty = val;
        flags_|=tb::unbox(Fields::BidQty);
        return *this;
    }
    const Qty bid_qty() const { return bid_.qty; }
    auto& ask_price(Price val) {
        ask_.price = val;
        flags_|=tb::unbox(Fields::AskPrice);
        return *this;
    }
    const Price ask_price() const { return ask_.price; }
    auto& ask_qty(Qty val) {
        ask_.qty = val;
        flags_|=tb::unbox(Fields::AskQty);
        return *this;
    }
    auto& price(TickSide side, Price val) {
        switch(side) {
            case TickSide::Buy:
                bid_price(val);
                break;
            case TickSide::Sell:
                ask_price(val);
        }
        return *this;
    }
    auto& qty(TickSide side, Qty val) {
        switch(side) {
            case TickSide::Buy:
                bid_qty(val);
                break;
            case TickSide::Sell:
                ask_qty(val);
        }
        return *this;
    }
    const Qty ask_qty() const { return ask_.qty; }
    template<class ValueT>
    auto& update(const ValueT& e) {
        qty(e.side(), e.qty());
        price(e.side(), e.price());
        return *this;
    }
    auto price_conv() const { return Policy().price_conv(); }

    friend std::ostream& operator<<(std::ostream& os, const BasicBestPrice& self) {
        os <<"t:'BestPrice'";
        if(self.contains(Fields::SendTime)) {
            os << ", ts:'"<<toolbox::sys::put_time<toolbox::Nanos>(self.send_time())<<"'";
            if(self.contains(Fields::RecvTime)) {
                os << ", clt:" << (self.recv_time()-self.send_time()).count();
            }
        }
        if(self.contains(Fields::BidPrice))
            os << ", bid:"<<self.price_conv().to_double(self.bid_price());
        if(self.contains(Fields::BidQty))
            os << ", bid_qty:"<<self.bid_qty();
        if(self.contains(Fields::BidPrice))
            os << ", ask:"<<self.price_conv().to_double(self.ask_price());
        if(self.contains(Fields::BidQty))
            os << ", ask_qty:"<<self.ask_qty();
        return os;
    }
protected:
    VenueInstrumentId venue_instrument_id_ {};
    tb::WallTime send_time_;
    tb::WallTime recv_time_;
    PriceQty bid_ {};
    PriceQty ask_ {};
    uint32_t flags_ {0};
};

using BestPrice = BasicBestPrice<typename core::Tick::Element::Policy>;

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