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

template<typename ProtocolT>
class SpbInstrumentStream : public BasicSpbStream<SpbInstrumentStream<ProtocolT>, core::InstrumentUpdate, ProtocolT>
{
public:
    using This = SpbInstrumentStream<ProtocolT>;
    using Base = BasicSpbStream<This, core::InstrumentUpdate, ProtocolT>;
    using typename Base::Protocol;
    using typename Base::Decoder;
    using typename Base::Schema;
    
    template<typename MessageT>
    using SpbPacket = typename Base::template SpbPacket<MessageT>;

    using InstrumentSnapshot = typename Schema::InstrumentSnapshot;
    using TypeList = mp::mp_list<InstrumentSnapshot>;

public:
    SpbInstrumentStream(Protocol& protocol)
    : Base(protocol, ft::core::StreamTopic::Instrument) 
    {
        //TOOLBOX_DUMP_THIS;
    }
    using Base::decoder;
    using Base::invoke;
    using Base::protocol;

    void on_packet(const SpbPacket<InstrumentSnapshot>& pkt) {
        //TOOLBOX_INFO << pkt;
        auto& d = pkt.value().value();
        core::BasicInstrumentUpdate<4096> u;
        u.symbol(d.symbol.str());
        u.exchange(protocol().exchange());
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
            params["files"].copy(snapshot_files_);
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
            exchange_symbol += protocol().exchange();
            std::size_t id = std::hash<std::string>{}(exchange_symbol);
            std::int64_t viid = std::atoll(e.attribute("instrument_id").value());

            core::BasicInstrumentUpdate<4096> u;
            u.topic(core::StreamTopic::Instrument);
            u.symbol(sym);
            u.exchange(protocol().exchange());
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