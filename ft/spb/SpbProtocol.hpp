#pragma once
#include "ft/utils/Common.hpp"
#include "SpbSchema.hpp"
#include "SpbDecoder.hpp"
#include "SpbBestPriceStream.hpp"



namespace ft::spb {

template<typename SchemaT>
class SpbProtocol
{
public:
    using Schema = SchemaT;
    using BestPriceStream = SpbBestPriceStream<Schema>;
    // TODO: consider merging typelist from BestPriceStream, OrderBookStream
    using TypeList = typename Schema::TypeList;
public:
    void decode(std::string_view data) { decoder_.decode(data); }
    BestPriceStream& bestprice() { return bestprice_; }
    auto& decoder() { return decoder_; }
private:
    SpbDecoder<Frame, TypeList> decoder_;
    BestPriceStream bestprice_;
};

using SpbUdpProtocol = SpbProtocol<SpbSchema<SpbUdp>>;

}