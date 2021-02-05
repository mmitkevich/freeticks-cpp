#pragma once

#include "ft/core/Fields.hpp"
#include "ft/core/InstrumentsCache.hpp"
#include "toolbox/util/Enum.hpp"

namespace ft { inline namespace core {


/// select * from spb_taq where LocalTime>(select addMinutes(max(LocalTime),-10) from spb_taq) limit 5

/// enumerate (subset) of fields of object
template<typename FieldT=core::Field>
class BasicFields  {
public:
    using Field = FieldT;
    
    BasicFields() = default;
    BasicFields(tb::BitMask<FieldT> fields=tb::BitMask<FieldT>{}.set())
    : fields_(fields) {}

    template<typename V, typename SinkT>
    bool try_sink(Field field, V obj, SinkT& sink) const {
        if(fields_.test(field)) {
            sink(field, std::move(obj));
            return true;
        }
        return false;
    };
    
    template<class T, typename SinkFn>
    void for_each(const T& obj, SinkFn&& sink) const {
        #define FT_SINK_FIELD_(fld, expr) \
        if constexpr(TB_IS_VALID(obj, expr)) { \
            try_sink(fld, expr, sink); \
        }
        FT_SINK_FIELD_(Field::InstrumentId, obj.instrument());
        if(instruments_) {
            FT_SINK_FIELD_(Field::Symbol, instruments_->symbol(obj.venue_instrument_id()));
        }
        FT_SINK_FIELD_(Field::Price, obj.price_conv().to_double(obj.price()));
        FT_SINK_FIELD_(Field::Qty, obj.qty_conv().to_double(obj.qty()));
        FT_SINK_FIELD_(Field::Side, obj.side());
        FT_SINK_FIELD_(Field::VenueInstrumentId, obj.venue_instrument_id());
        FT_SINK_FIELD_(Field::Seq, obj.sequence());
        FT_SINK_FIELD_(Field::Exchange, obj.exchange());
        FT_SINK_FIELD_(Field::VenueSymbol, obj.venue_symbol());
        FT_SINK_FIELD_(Field::LocalTime, obj.recv_time());
        FT_SINK_FIELD_(Field::Time, obj.send_time());
        FT_SINK_FIELD_(Field::BidPrice, obj.price_conv().to_double(obj.bid_price()));
        FT_SINK_FIELD_(Field::AskPrice, obj.price_conv().to_double(obj.ask_price()));
        FT_SINK_FIELD_(Field::BidQty, obj.qty_conv().to_double(obj.bid_qty()));
        FT_SINK_FIELD_(Field::AskQty, obj.qty_conv().to_double(obj.ask_qty()));
        FT_SINK_FIELD_(Field::LastPrice, obj.price_conv().to_double(obj.last_price()));        
        FT_SINK_FIELD_(Field::LastQty, obj.qty_conv().to_double(obj.last_qty()));
        FT_SINK_FIELD_(Field::Event, obj.event());
        FT_SINK_FIELD_(Field::Empty, nullptr);  // separator
    }
    static Field from_string(std::string_view s) {
        if(s=="BidPrice")
            return Field::BidPrice;
        if(s=="BidQty")
            return Field::BidQty;
        if(s=="AskPrice")
            return Field::AskPrice;
        if(s=="AskQty")
            return Field::AskQty;
        if(s=="Time")
            return Field::Time;
        if(s=="LocalTime")
            return Field::LocalTime;
        if(s=="InstrumentId")
            return Field::InstrumentId;
        if(s=="Symbol")
            return Field::Symbol;
        if(s=="LastPrice")
            return Field::LastPrice;
        if(s=="LastQty")
            return Field::LastQty;
        if(s=="VenueInstrumentId")
            return Field::VenueInstrumentId;
        if(s=="Exchange")
            return Field::Exchange;
        if(s=="VenueSymbol")
            return Field::VenueSymbol;
        if(s=="Price")
            return Field::Price;
        if(s=="Qty")
            return Field::Qty;
        if(s=="Side")
            return Field::Side;
        if(s=="Event")
            return Field::Event;
        if(s=="Topic")
            return Field::Topic;
        return Field::Empty;
    }
    tb::BitMask<Field>& enabled() { return fields_; }
    void instruments(core::InstrumentsCache* val) {
        instruments_ = val;
    }
    core::InstrumentsCache* instruments() { return instruments_; }
protected:
    tb::BitMask<Field> fields_;
    core::InstrumentsCache* instruments_{};
};

template<class Base=BasicFields<core::Field>>
class JoinFields : public Base {
public:
    using Base::Base;
    using typename Base::Field;
    template<class T, typename SinkFn>
    void for_each(const T& obj, SinkFn&& sink) const {
        //if constexpr(TB_IS_VALID(obj, obj.begin())) {
            // collection
            for(auto& child: obj) {
                tb::BitMask<Field> sent;
                auto skip_empty = [&sink, &sent](auto field, const auto& val) {
                    if(field!=Field::Empty) {
                        sink(field, val);
                        sent.set(field);
                    }
                };
                auto skip_sent = [&sink, &sent](auto field, const auto& val) {
                    if(!sent.test(field))   // resolve ambiguity using stuff from child in first place
                        sink(field, val);
                };
                Base::for_each(child, skip_empty);            
                Base::for_each(obj, skip_sent);
            }
        //} else {
        //    Base::for_each(obj, sink);
        //}
    }
};

}}