#pragma once

#include "ft/core/Fields.hpp"
#include "ft/core/InstrumentsCache.hpp"
#include "toolbox/util/Enum.hpp"
#include <type_traits>

namespace ft { inline namespace core {

class SinkTag {};

/// select * from spb_taq where LocalTime>(select addMinutes(max(LocalTime),-10) from spb_taq) limit 5

/// write field if expr(obj) is valid
#define FT_SINK_IF_VALID(fld, expr) \
if constexpr(TB_IS_VALID(obj, expr)) { \
    fn(fld, obj, [&](auto& obj) { return expr; }); \
}

/// enumerate (subset) of fields of object
template<class Self, class FieldT=core::Field, class...>
class BasicSerializer  {
    FT_SELF(Self);
public:
    using Field = FieldT;

    BasicSerializer(tb::BitMask<FieldT> fields=tb::BitMask<FieldT>{}.set())
    : fields_(fields) {}

    template<class SinkT, class T>
    void write_header(SinkT& sink, tb::type_c<T> type) {
        self()->for_each(sink, *(T*)nullptr, [&](Field field, const T& obj, auto&& value_fn) {
            if(fields_.test(field)) {
                sink.write_header(field);
            }
        });
    }

    template<class SinkT, class T>
    void write(SinkT& sink, const T& obj) const {
        self()->for_each(sink, obj, [&](Field field, const T& obj, auto&& value_fn) {
            if(fields_.test(field)) {
                sink.write_field(field, std::move(value_fn(obj)));
            }
        });
    }

    template<class SinkT, class T, class Fn>
    void for_each(SinkT& sink, const T& obj, Fn&& fn) const {
        FT_SINK_IF_VALID(Field::InstrumentId, obj.instrument());
        if(instruments_) {
            FT_SINK_IF_VALID(Field::Symbol, instruments_->symbol(obj.venue_instrument_id()));
        }
        FT_SINK_IF_VALID(Field::Price, obj.price_conv().to_double(obj.price()));
        FT_SINK_IF_VALID(Field::Qty, obj.qty_conv().to_double(obj.qty()));
        FT_SINK_IF_VALID(Field::Side, obj.side());
        FT_SINK_IF_VALID(Field::VenueInstrumentId, obj.venue_instrument_id());
        FT_SINK_IF_VALID(Field::Seq, obj.sequence());
        FT_SINK_IF_VALID(Field::Exchange, obj.exchange());
        FT_SINK_IF_VALID(Field::VenueSymbol, obj.venue_symbol());
        FT_SINK_IF_VALID(Field::LocalTime, obj.recv_time());
        FT_SINK_IF_VALID(Field::Time, obj.send_time());
        FT_SINK_IF_VALID(Field::BidPrice, obj.price_conv().to_double(obj.bid_price()));
        FT_SINK_IF_VALID(Field::AskPrice, obj.price_conv().to_double(obj.ask_price()));
        FT_SINK_IF_VALID(Field::BidQty, obj.qty_conv().to_double(obj.bid_qty()));
        FT_SINK_IF_VALID(Field::AskQty, obj.qty_conv().to_double(obj.ask_qty()));
        FT_SINK_IF_VALID(Field::LastPrice, obj.price_conv().to_double(obj.last_price()));        
        FT_SINK_IF_VALID(Field::LastQty, obj.qty_conv().to_double(obj.last_qty()));
        FT_SINK_IF_VALID(Field::Event, obj.event());
        sink.write_eol();
    }

    Field from_string(std::string_view s) const {
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

template<class FieldT=core::Field>
class Serializer: public BasicSerializer<Serializer<FieldT>, FieldT> {
    using Base = BasicSerializer<Serializer<FieldT>,FieldT>;
public:
    using Base::Base;
};

/// { a: {b:1,c:2}, b:3, c:4, d:5} => {b:1, c:2, d:5}
template<class Self, typename FieldT=core::Field, template<class, class...> class BaseM = BasicSerializer>
class BasicFlatSerializer : public BaseM<Self, FieldT> {
    using Base = BaseM<Self,FieldT>;
    FT_SELF(Self)
public:
    using Base::Base;
    using typename Base::Field;

    template<class SinkT>
    struct UniqueSink : SinkTag {
        UniqueSink(SinkT& sink)
        : sink(sink)
        {}
        
        void write_eol() { } // concat lines
        void write_header(Field fld) { 
             if(!sent.test(fld)) {
                sink.write_header(fld);
                sent.set(fld);
            }
        }
        template<class Field, class V>
        void write_field(Field fld, V val) {
            if(!sent.test(fld)) {
                sink.write_field(fld, val);
                sent.set(fld);
            }
        }
        tb::BitMask<Field> sent;
        SinkT& sink;
        bool pass_eol=true;
    };
    template<class SinkT, class T>
    void write(SinkT& sink, const T& obj) const {
        if constexpr(TB_IS_VALID(obj, obj.begin()) && TB_IS_VALID(obj, obj.end())) {
            // collection
            for(auto& child: obj) {
                UniqueSink unique(sink);
                self()->write(unique, child); // write child fields
                Base::write(unique, obj); // write our fields (skipping found in the child)
                sink.write_eol();
            }
        } else {
            Base::write(sink, obj);
        }
    }

    template<class SinkT, class T>
    void write_header(SinkT& sink, tb::type_c<T> type) {
        static_assert(std::is_base_of<SinkTag, SinkT>::value);

        T& obj = *(T*)nullptr;
        if constexpr(TB_IS_VALID(obj, obj.begin()) && TB_IS_VALID(obj, obj.end())) {
            UniqueSink unique(sink);
            using E = std::decay_t<decltype(*obj.begin())>;
            self()->write_header(unique, tb::type_c<E>{});
            Base::write_header(unique, type);
            sink.write_eol();
        } else {
            Base::write_header(sink, type);
        }
    }
};

template<class FieldT=core::Field, template<class Self,class Field, class...> class BaseM = BasicSerializer>
class FlatSerializer: public BasicFlatSerializer<FlatSerializer<FieldT, BaseM>, FieldT, BaseM> {
    using Base = BasicFlatSerializer<FlatSerializer<FieldT, BaseM>, FieldT, BaseM>;
public:
    using Base::Base;
};

}}