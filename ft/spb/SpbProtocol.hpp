#pragma once
#include "ft/core/Instrument.hpp"
#include "ft/core/Tick.hpp"
#include "ft/spb/SpbFrame.hpp"
#include "ft/utils/Common.hpp"
#include "SpbSchema.hpp"
#include "SpbDecoder.hpp"
#include "SpbBestPriceStream.hpp"
#include "SpbInstrumentStream.hpp"
#include <cstdint>


namespace ft::spb {


template<
typename SchemaT,
typename BinaryPacketT,
typename DecoderT = SpbDecoder<spb::Frame, SchemaT, BinaryPacketT>
> class SpbProtocol
{
public:
    using This = SpbProtocol<SchemaT, BinaryPacketT, DecoderT>;
    using Schema = SchemaT;
    using BinaryPacket = BinaryPacketT;
    using TypeList = typename Schema::TypeList;     // TODO: consider merging typelist from BestPriceStream, OrderBookStream
    using Decoder = DecoderT;
    using BestPriceStream = SpbBestPriceStream<This>;
    using InstrumentStream = SpbInstrumentStream<This>;
    using PriceScale = core::PriceScale<100'000'000, core::CorePriceMultiplier>;
    static constexpr std::string_view Exchange = "SPB";
    static constexpr std::string_view Venue = "SPB_MDBIN";
public:
    Decoder& decoder() { return decoder_; }
    auto& stats() {return decoder().stats(); }
    void on_packet(BinaryPacket e) { decoder_.on_packet(e); }
    
    core::TickStream& bestprice() { return bestprice_; }
    
    core::VenueInstrumentStream& instrument() { return instrument_; }

    std::string_view exchange() { return Exchange; }
    std::string_view venue() { return Venue; }
    PriceScale price_scale() { return PriceScale();}
private:
    Decoder decoder_;
    BestPriceStream bestprice_ {*this};
    InstrumentStream instrument_ {*this};
};

template<typename BinaryPacketT>
using SpbUdpProtocol = SpbProtocol<SpbSchema<SpbUdp>, BinaryPacketT>;

}