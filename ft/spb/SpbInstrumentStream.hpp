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
    using Base::Base;
    using Base::decoder;
    using Base::invoke;
    using Base::protocol;

    static constexpr std::string_view name() { return "instrument"; }

    void on_packet(const SpbPacket<InstrumentSnapshot>& pkt) {
        //TOOLBOX_INFO << pkt;
        auto& d = pkt.value().value();
        core::VenueInstrument vi;
        vi.venue(protocol().venue());
        vi.exchange(protocol().exchange());
        vi.instrument().symbol(d.symbol.str());
        auto id = std::hash<std::string>{}(vi.venue_symbol());
        vi.id(id);
        vi.venue_instrument_id(d.instrument_id);
        invoke(vi);
    }
    
    void on_parameters_updated(const core::Parameters& params) {
        Base::on_parameters_updated(params);
        std::string_view type = params["type"].get_string();
        if(type == "snapshot.xml") {
            for(auto e:params["urls"]) {
                std::string url = std::string{e.get_string()};
                snapshot_xml_url_ = url;
                //parent().state_hook(core::State::Started, [url, this] { snapshot_xml(url); });
            }
        }
    }
    void open() {
        if(!snapshot_xml_url_.empty())
            snapshot_xml(snapshot_xml_url_);
    }
    void snapshot_xml(std::string_view path) {
        tb::xml::Parser p;
        tb::xml::MutableDocument d = p.parse_file(path);
        core::VenueInstrument vi;
        vi.venue(protocol().venue());
        vi.exchange(protocol().exchange());
        for(auto e:d.child("exchange").child("traded_instruments")) {
            std::string_view sym = e.attribute("symbol").value();
            std::string_view is_test = std::string_view(e.attribute("is_test").value());
            vi.instrument().symbol(sym);
            auto id = std::hash<std::string>{}(vi.venue_symbol());
            vi.id(id);
            std::int64_t vid = std::atoll(e.attribute("instrument_id").value());
            vi.venue_instrument_id(vid);
            if(is_test!="true")
                invoke(vi);
        }
    }
private:
    std::string snapshot_xml_url_;
};

}