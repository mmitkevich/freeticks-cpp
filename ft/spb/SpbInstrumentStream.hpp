#pragma once
#include "ft/utils/Common.hpp"
#include "ft/core/Stream.hpp"
#include "ft/core/Tick.hpp"
#include "ft/spb/SpbSchema.hpp"
#include "ft/spb/SpbDecoder.hpp"
namespace ft::spb {

template<typename DecoderT>
class SpbInstrumentStream : public core::TickStream
{
public:
    using Base = core::Stream<core::Tick>;
    using This = SpbInstrumentStream<DecoderT>;
    using Decoder = DecoderT;
    using Schema = typename Decoder::Schema;
    using InstrumentSnapshot = typename Schema::InstrumentSnapshot;
    template<typename MessageT>
    using TypedPacket = typename Decoder::template TypedPacket<MessageT>;
public:
    SpbInstrumentStream(Decoder & decoder)
    :   decoder_(decoder) {
        connect();
    }
    ~SpbInstrumentStream() {
        disconnect();
    }
protected:
    void connect() {
        decoder_.signals().connect(tbu::bind<&This::on_instrument>(this));
    }
    void disconnect() {
        decoder_.signals().disconnect(tbu::bind<&This::on_instrument>(this));
    }  
    void on_instrument(TypedPacket<InstrumentSnapshot> e) {
        TOOLBOX_INFO << e;
    }
private:
    Decoder& decoder_;
};

}