#pragma once

#include "ft/core/Tick.hpp"
#include <boost/mp11/bind.hpp>
#include <ft/core/Identifiable.hpp>
#include <ft/core/Fields.hpp>

namespace ft { inline namespace core {

template<typename PolicyT>
class BasicBestPrice 
{
public:
    using Side = core::TickSide;
    using Policy = PolicyT;
public:
    InstrumentId instrument_id() const { return instrument_id_; }
    void instrument_id(InstrumentId val) {
        instrument_id_ = val;
        fields_.set(Field::InstrumentId);
    }
    VenueInstrumentId venue_instrument_id() const { return venue_instrument_id_; }
    void venue_instrument_id(VenueInstrumentId val) { 
        venue_instrument_id_ = val;
        fields_.set(Field::VenueInstrumentId);
    }
    bool empty() const { return fields_.none(); }
    void clear() { 
        fields_.reset();
        ask_ = {};
        bid_ = {};
        last_ = {};
    }
    void send_time(Timestamp time) { 
        send_time_ = time; 
        fields_.set(Field::Time);
    }
    Timestamp send_time() const { return send_time_; }
    void recv_time(Timestamp time) { 
        recv_time_ = time; 
        fields_.set(Field::LocalTime);
    }
    Timestamp recv_time() const { return recv_time_; }

    auto& bid_price(Price val) { 
        bid_.price = val;
        fields_.set(Field::BidPrice);
        return *this;
    }
    const Price bid_price() const { return bid_.price; }
    auto& bid_qty(Qty val) {
        bid_.qty = val;
        fields_.set(Field::BidQty);
        return *this;
    }
    const Qty bid_qty() const { return bid_.qty; }
    
    auto& ask_price(Price val) {
        ask_.price = val;
        fields_.set(Field::AskPrice);
        return *this;
    }
    const Price ask_price() const { return ask_.price; }
    auto& ask_qty(Qty val) {
        ask_.qty = val;
        fields_.set(Field::AskQty);
        return *this;
    }
    const Qty ask_qty() const { return ask_.qty; }
    
    auto& last_price(Price val) {
        last_.price = val;
        fields_.set(Field::LastPrice);
        return *this;
    }
    const Price last_price() const { return last_.price; }
    auto& last_qty(Qty val) {
        last_.qty = val;
        fields_.set(Field::LastQty);
        return *this;
    }
    const Qty last_qty() const { return last_.qty; }
    
    const Timestamp last_time() const { return last_.time; }
    auto& last_time(Timestamp val) {
        last_.time = val;
        fields_.set(Field::LastTime);
        return *this;
    }

    auto& price(Side side, Price val) {
        switch(side) {
            case Side::Buy:
                bid_price(val);
                break;
            case Side::Sell:
                ask_price(val);
        }
        return *this;
    }
    auto& qty(Side side, Qty val) {
        switch(side) {
            case Side::Buy:
                bid_qty(val);
                break;
            case Side::Sell:
                ask_qty(val);
        }
        return *this;
    }

    template<class ValueT>
    auto& update(const ValueT& e) {
        switch(e.event()) {
            case TickEvent::Modify: {
                qty(e.side(), e.qty());
                price(e.side(), e.price());            
            } break;
            case TickEvent::Fill: {
                last_qty(e.qty());
                last_price(e.price());
                last_time(send_time()); // take from send_time
                break;
            }
            default: TOOLBOX_DUMP << "ignoring evt:"<<e.event();            
        } 
        return *this;
    }
    auto price_conv() const { return Policy().price_conv(); }

    friend std::ostream& operator<<(std::ostream& os, const BasicBestPrice& self) {
        os <<"t:'BestPrice'";
        if(self.test(Field::Time)) {
            os << ", time:'"<<toolbox::sys::put_time<toolbox::Nanos>(self.send_time())<<"'";
            if(self.test(Field::LocalTime)) {
                os << ", clt:" << (self.recv_time()-self.send_time()).count();
            }
        }
        if(self.test(Field::BidPrice))
            os << ", bid:"<<self.price_conv().to_double(self.bid_price());
        if(self.test(Field::BidQty))
            os << ", bid_qty:"<<self.bid_qty();
        if(self.test(Field::BidPrice))
            os << ", ask:"<<self.price_conv().to_double(self.ask_price());
        if(self.test(Field::BidQty))
            os << ", ask_qty:"<<self.ask_qty();
        return os;
    }
    /// returns true if field is set
    bool test(core::Field field) const { return fields_.test(field); } 
    
    /// enumerates all possible fields in Best Price
    template<typename FieldT=core::Field>
    constexpr static tb::BitMask<FieldT> fields() {
        return {
            Field::VenueInstrumentId,
            Field::LocalTime,
            Field::Time,
            Field::BidPrice,
            Field::BidQty,
            Field::AskPrice,
            Field::AskQty,
            Field::LastPrice,
            Field::LastQty,
            Field::LastTime
        };
    }
protected:
    InstrumentId instrument_id_ {};
    VenueInstrumentId venue_instrument_id_ {};
    Timestamp send_time_;
    Timestamp recv_time_;
    PriceQtyTime bid_ {};
    PriceQtyTime ask_ {};
    PriceQtyTime last_ {};
    tb::BitMask<core::Field> fields_;
};

using BestPrice = BasicBestPrice<typename core::Tick::Element::Policy>;


}} // ft::core