#pragma once
#include "ft/core/Component.hpp"
#include "ft/utils/Common.hpp"
#include "ft/core/Stream.hpp"
#include "ft/core/Tick.hpp"
#include "ft/spb/SpbSchema.hpp"
#include "ft/spb/SpbDecoder.hpp"
#include "ft/core/Instrument.hpp"
#include "toolbox/util/Xml.hpp"

namespace ft::spb {

template<typename ProtocolT>
class SpbInstrumentStream : public BasicSpbStream<SpbInstrumentStream<ProtocolT>, ProtocolT, core::InstrumentStream>
{
public:
    using This = SpbInstrumentStream<ProtocolT>;
    using Base = BasicSpbStream<This, ProtocolT, core::InstrumentStream>;
    using Protocol = typename Base::Protocol;
    using Decoder = typename Base::Decoder;
    using Schema = typename Base::Schema;
    template<typename MessageT>
    using SpbPacket = typename Base::template SpbPacket<MessageT>;

    using InstrumentSnapshot = typename Schema::InstrumentSnapshot;
    using TypeList = mp::mp_list<InstrumentSnapshot>;

public:
    SpbInstrumentStream(Protocol& protocol)
    : Base(protocol) 
    {
        TOOLBOX_DUMP_THIS;
    }
    using Base::decoder;
    using Base::invoke;
    using Base::protocol;

    static constexpr std::string_view name() { return "instrument"; }

    void on_packet(const SpbPacket<InstrumentSnapshot>& pkt) {
        //TOOLBOX_INFO << pkt;
        auto& d = pkt.value().value();
        core::BasicInstrumentUpdate<4096> u;
        u.symbol(d.symbol.str());
        u.exchange(protocol().exchange());
        auto id = std::hash<std::string>{}(u.exchange_symbol());
        u.instrument_id(id);
        u.venue_instrument_id(d.instrument_id);
        invoke(u.as_size<0>());
    }
    
    void on_parameters_updated(const core::Parameters& params) {
        Base::on_parameters_updated(params);
        std::string_view type = params["type"].get_string();
        if(type == "snapshot.xml") {
            for(auto e:params["urls"]) {
                std::string url = std::string{e.get_string()};
                TOOLBOX_DUMP_THIS;
                snapshot_xml_url_ = url;
                //parent().state_hook(core::State::Started, [url, this] { snapshot_xml(url); });
            }
        }
    }
    void open() {
        TOOLBOX_DUMP_THIS;
        if(!snapshot_xml_url_.empty())
            snapshot_xml(snapshot_xml_url_);
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
            auto id = std::hash<std::string>{}(exchange_symbol);
            std::int64_t viid = std::atoll(e.attribute("instrument_id").value());

            core::BasicInstrumentUpdate<4096> u;
            u.topic(core::StreamTopic::Instrument);
            u.symbol(sym);
            u.exchange(protocol().exchange());
            u.venue_symbol(sym);
            u.instrument_id(id);    // hash function of symbol@exchange
            u.instrument_type(InstrumentType::Stock);
            u.venue_instrument_id(viid);

            if(is_test!="true")
                invoke(u.as_size<0>());
        }
        TOOLBOX_DEBUG<<"done loading snapshot file "<<path;
    }
private:
    std::string snapshot_xml_url_;
};

}