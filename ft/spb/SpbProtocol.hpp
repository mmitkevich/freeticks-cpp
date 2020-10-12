#pragma once
#include "ft/spb/SpbFrame.hpp"
#include "ft/utils/Common.hpp"
#include "SpbSchema.hpp"
#include "SpbDecoder.hpp"
#include "SpbBestPriceStream.hpp"
#include "SpbInstrumentStream.hpp"


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
    using BestPriceStream = SpbBestPriceStream<Decoder>;
    using InstrumentStream = SpbInstrumentStream<Decoder>;
public:
    Decoder& decoder() { return decoder_; }
    auto& stats() {return decoder().stats(); }
    void on_packet(BinaryPacket e) { decoder_.on_packet(e); }
    BestPriceStream& bestprice() { return bestprice_; }
private:
    Decoder decoder_;
    BestPriceStream bestprice_ {decoder_};
    InstrumentStream instrument_ {decoder_};
};

template<typename BinaryPacketT>
using SpbUdpProtocol = SpbProtocol<SpbSchema<SpbUdp>, BinaryPacketT>;

}