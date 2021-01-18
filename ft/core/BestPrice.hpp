#pragma once

#include "ft/core/Tick.hpp"
#include <ft/core/Identifiable.hpp>
#include <ft/core/Fields.hpp>

namespace ft { inline namespace core {


template<typename PolicyT>
class BasicBestPrice : public BasicFieldsWriter<BasicBestPrice<PolicyT>, core::Fields> {
    using Base = BasicFieldsWriter<BasicBestPrice<PolicyT>, core::Fields>;
public:
    using Policy = PolicyT;
    using typename Base::Fields;
    using Side = core::TickSide;
public:
    using Base::fields_mask, Base::contains_field;
    InstrumentId instrument_id() const { return instrument_id_; }
    void instrument_id(InstrumentId val) {
        instrument_id_ = val;
        fields_mask() |= tb::unbox(Fields::InstrumentId);
    }
    VenueInstrumentId venue_instrument_id() const { return venue_instrument_id_; }
    void venue_instrument_id(VenueInstrumentId val) { 
        venue_instrument_id_ = val;
        fields_mask() |= tb::unbox(Fields::VenueInstrumentId);
    }
    bool empty() const { return fields_mask() ==0; }
    void clear() { 
        fields_mask() = 0;
        ask_ = {};
        bid_ = {};
        last_ = {};
    }
    void send_time(Timestamp time) { 
        send_time_ = time; 
        fields_mask() |= tb::unbox(Fields::Time);
    }
    Timestamp send_time() const { return send_time_; }
    void recv_time(Timestamp time) { 
        recv_time_ = time; 
        fields_mask() |= tb::unbox(Fields::LocalTime);
    }
    Timestamp recv_time() const { return recv_time_; }

    auto& bid_price(Price val) { 
        bid_.price = val;
        fields_mask() |= tb::unbox(Fields::BidPrice);
        return *this;
    }
    const Price bid_price() const { return bid_.price; }
    auto& bid_qty(Qty val) {
        bid_.qty = val;
        fields_mask() |= tb::unbox(Fields::BidQty);
        return *this;
    }
    const Qty bid_qty() const { return bid_.qty; }
    
    auto& ask_price(Price val) {
        ask_.price = val;
        fields_mask() |= tb::unbox(Fields::AskPrice);
        return *this;
    }
    const Price ask_price() const { return ask_.price; }
    auto& ask_qty(Qty val) {
        ask_.qty = val;
        fields_mask() |= tb::unbox(Fields::AskQty);
        return *this;
    }
    const Qty ask_qty() const { return ask_.qty; }
    
    auto& last_price(Price val) {
        last_.price = val;
        fields_mask() |= tb::unbox(Fields::LastPrice);
        return *this;
    }
    const Price last_price() const { return last_.price; }
    auto& last_qty(Qty val) {
        last_.qty = val;
        fields_mask() |= tb::unbox(Fields::LastQty);
        return *this;
    }
    const Qty last_qty() const { return last_.qty; }
    
    const Timestamp last_time() const { return last_.time; }
    auto& last_time(Timestamp val) {
        last_.time = val;
        fields_mask() |= tb::unbox(Fields::LastTime);
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

    static std::vector<Fields> fields() {
        return {Fields::VenueInstrumentId, Fields::LocalTime, Fields::Time, Fields::BidPrice, Fields::BidQty, Fields::AskPrice, Fields::AskQty, Fields::LastPrice, Fields::LastQty, Fields::LastTime };
    }

    void write_field(std::ostream& os, Fields field) const {
        if(contains_field(field)) {
            switch(field) {
                case Fields::BidPrice: os << price_conv().to_double(bid_price()); break;
                case Fields::BidQty: os << bid_qty(); break;
                case Fields::AskPrice: os << price_conv().to_double(ask_price()); break;
                case Fields::AskQty: os << ask_qty(); break;
                case Fields::LastPrice: os << price_conv().to_double(last_price()); break;
                case Fields::LastQty: os << last_qty(); break;
                case Fields::LastTime: os << toolbox::sys::put_time<toolbox::Nanos>(last_time()); break;
                default: Base::write_field(os, field); break;
            }
        } else {
            //os << "<empty>";
        }
    }
    friend std::ostream& operator<<(std::ostream& os, const BasicBestPrice& self) {
        os <<"t:'BestPrice'";
        if(self.contains_field(Fields::Time)) {
            os << ", time:'"<<toolbox::sys::put_time<toolbox::Nanos>(self.send_time())<<"'";
            if(self.contains_field(Fields::LocalTime)) {
                os << ", clt:" << (self.recv_time()-self.send_time()).count();
            }
        }
        if(self.contains_field(Fields::BidPrice))
            os << ", bid:"<<self.price_conv().to_double(self.bid_price());
        if(self.contains_field(Fields::BidQty))
            os << ", bid_qty:"<<self.bid_qty();
        if(self.contains_field(Fields::BidPrice))
            os << ", ask:"<<self.price_conv().to_double(self.ask_price());
        if(self.contains_field(Fields::BidQty))
            os << ", ask_qty:"<<self.ask_qty();
        return os;
    }
protected:
    InstrumentId instrument_id_ {};
    VenueInstrumentId venue_instrument_id_ {};
    Timestamp send_time_;
    Timestamp recv_time_;
    PriceQtyTime bid_ {};
    PriceQtyTime ask_ {};
    PriceQtyTime last_ {};
};

using BestPrice = BasicBestPrice<typename core::Tick::Element::Policy>;


}} // ft::core