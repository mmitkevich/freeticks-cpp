#pragma once
#include "ft/core/Component.hpp"
#include "ft/utils/Common.hpp"
#include "ft/core/Stream.hpp"
#include "ft/core/Tick.hpp"
#include "ft/spb/SpbSchema.hpp"
#include "ft/spb/SpbDecoder.hpp"
#include "ft/core/Instrument.hpp"
#include "toolbox/util/Xml.hpp"
#include <iterator>

namespace ft::spb {

template<class SchemaT, typename...O>
class SpbInstrumentStream : public BasicSpbStream<
    SpbInstrumentStream<SchemaT, O...>
    , core::InstrumentUpdate, SchemaT::template Channel>
{
public:
    using Self = SpbInstrumentStream<SchemaT, O...>;
    using Base = BasicSpbStream<Self, core::InstrumentUpdate, SchemaT::template Channel>;
    using Schema = SchemaT;

    using InstrumentSnapshot = typename Schema::InstrumentSnapshot;
    using TypeList = mp::mp_list<InstrumentSnapshot>;

public:
    SpbInstrumentStream()
    : Base(StreamTopic::Instrument) {}

    using Base::invoke;

    template<class PacketT>
    void on_packet(const PacketT& pkt) {
        //TOOLBOX_INFO << pkt;
        auto& d = pkt.value().value();
        core::BasicInstrumentUpdate<4096> u;
        u.symbol(d.symbol.str());
        u.exchange(Schema::Exchange);
        u.venue_symbol(d.symbol.str());
        auto id = std::hash<std::string>{}(u.exchange_symbol());
        u.instrument_id(Identifier(id));
        u.venue_instrument_id(Identifier(d.instrument_id));
        invoke(std::move(u.as_size<0>()));
    }
    
    void on_parameters_updated(const core::Parameters& params) {
        Base::on_parameters_updated(params);
        std::string_view type = params.strv("type");
        if(type == "snapshot") {
            params["resource"].copy(snapshot_files_);
        }
    }
    void open() {
        //TOOLBOX_DUMP_THIS;
        for(auto& file: snapshot_files_)
            snapshot_xml(file);
    }
    void snapshot_xml(std::string_view path) {
        TOOLBOX_DEBUG<<"start load snapshot file "<<path;
        tb::xml::Parser p;
        tb::xml::MutableDocument d = p.parse_file(path);
        for(auto e:d.child("exchange").child("traded_instruments")) {
            std::string_view sym = e.attribute("symbol").value();
            std::string_view is_test = std::string_view(e.attribute("is_test").value());
            
            auto exchange_symbol = std::string{sym};
            exchange_symbol += "@";
            exchange_symbol += Schema::Exchange;
            std::size_t id = std::hash<std::string>{}(exchange_symbol);
            std::int64_t viid = std::atoll(e.attribute("instrument_id").value());

            core::BasicInstrumentUpdate<4096> u;
            u.topic(core::StreamTopic::Instrument);
            u.symbol(sym);
            u.exchange(Schema::Exchange);
            u.venue_symbol(sym);
            u.instrument_id(Identifier(id));    // hash function of symbol@exchange
            u.instrument_type(InstrumentType::Stock);
            u.venue_instrument_id(Identifier(viid));

            if(is_test!="true") {
                invoke(std::move(u.as_size<0>()));
            }
        }
        TOOLBOX_DEBUG<<"done loading snapshot file "<<path;
    }
private:
    std::vector<std::string> snapshot_files_;
};

}