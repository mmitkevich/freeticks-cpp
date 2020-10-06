#pragma once
#include "ft/spb/SpbFrame.hpp"
#include "ft/utils/Common.hpp"
#include "SpbSchema.hpp"
#include "SpbDecoder.hpp"
#include "SpbBestPriceStream.hpp"
#include <sstream>

namespace ft::spb {

template<typename SchemaT>
class SpbProtocol
{
public:
    using Schema = SchemaT;
    using TypeList = typename Schema::TypeList;     // TODO: consider merging typelist from BestPriceStream, OrderBookStream
    using Decoder = SpbDecoder<Frame, TypeList>;
    using BestPriceStream = SpbBestPriceStream<Schema>;
public:
    void decode(std::string_view data) { decoder_.decode(data); }
    BestPriceStream& bestprice() { return bestprice_; }
    auto& decoder() { return decoder_; }
    std::string to_string(const Instrument& ins) {
        std::stringstream ss;
        ss << ins.insturmentid << "_" << ins.marketid;
        return ss.str();
    }
private:
    Decoder decoder_;
    BestPriceStream bestprice_ {*this};
};

using SpbUdpProtocol = SpbProtocol<SpbSchema<SpbUdp>>;

}