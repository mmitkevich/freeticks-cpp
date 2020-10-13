#pragma once
#include "ft/utils/Common.hpp"
#include "ft/core/Stream.hpp"
#include "ft/core/Tick.hpp"
#include "ft/spb/SpbSchema.hpp"
#include "ft/spb/SpbDecoder.hpp"
#include "ft/core/Instrument.hpp"

namespace ft::spb {

template<typename ProtocolT>
class SpbInstrumentStream : public core::VenueInstrumentStream
{
public:
    using Base = core::Stream<core::Tick>;
    using This = SpbInstrumentStream<ProtocolT>;
    using Protocol = ProtocolT;
    using Decoder = typename Protocol::Decoder;
    using Schema = typename Decoder::Schema;
    using InstrumentSnapshot = typename Schema::InstrumentSnapshot;
    template<typename MessageT>
    using TypedPacket = typename Decoder::template TypedPacket<MessageT>;
public:
    SpbInstrumentStream(Protocol & protocol)
    :   protocol_(protocol) {
        connect();
    }
    ~SpbInstrumentStream() {
        disconnect();
    }
    Decoder& decoder() { return protocol_.decoder(); }
protected:
    void connect() {
        decoder().signals().connect(tbu::bind<&This::on_instrument>(this));
    }
    void disconnect() {
        decoder().signals().disconnect(tbu::bind<&This::on_instrument>(this));
    }  
    void on_instrument(TypedPacket<InstrumentSnapshot> e) {
        TOOLBOX_INFO << e;
        auto& snap = *e.data();
        core::VenueInstrument vi;
        vi.venue(protocol_.venue());
        vi.exchange(protocol_.exchange());
        vi.instrument().symbol(snap.symbol.str());
        auto id = std::hash<std::string>{}(vi.venue_symbol());
        vi.id(id);
        vi.venue_instrument_id(snap.instrument_id);
        invoke(vi);
    }
private:
    Protocol& protocol_;
};

}