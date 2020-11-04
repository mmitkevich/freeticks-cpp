#pragma once
#include "ft/core/Executor.hpp"
#include "ft/core/Instrument.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/Tick.hpp"
#include "ft/spb/SpbFrame.hpp"
#include "ft/utils/Common.hpp"
#include "SpbSchema.hpp"
#include "SpbDecoder.hpp"
#include "SpbBestPriceStream.hpp"
#include "SpbInstrumentStream.hpp"
#include "toolbox/util/Tuple.hpp"
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <cstdint>
#include <sstream>
#include <string_view>
#include <vector>


namespace ft::spb {

template <class T> struct tuple_construct_t;

template <class... Ts> struct tuple_construct_t<std::tuple<Ts...>> {
  template <class... Args>
  static std::tuple<Ts...> make_tuple(Args&&... args) {
    //this is the central part - the double pack expansion
    return std::make_tuple(Ts{args...}...);
  }
};

// a little free helper function...
template <class Tup, class... Args>
Tup construct_tuple(Args&&... args) {
    return tuple_construct_t<Tup>::make_tuple(std::forward<Args>(args)...);
}

template<
typename SchemaT,
typename BinaryPacketT
> class SpbProtocol
{
public:
    using Schema = SchemaT;
    using BinaryPacket = BinaryPacketT;
    using This = SpbProtocol<Schema, BinaryPacket>;
    using BestPriceStream = SpbBestPriceStream<This>;
    using InstrumentStream = SpbInstrumentStream<This>;
    using StreamsTuple = std::tuple<BestPriceStream&, InstrumentStream&>;
    using Decoder = SpbDecoder<spb::Frame, SchemaT, BinaryPacketT, StreamsTuple>;

    static constexpr std::int64_t PriceMultiplier = 100'000'000;
    using PriceConv = core::PriceConversion<std::int64_t, PriceMultiplier>;
    using QtyConv = core::QtyConversion<std::int64_t, 1>;

    static constexpr std::string_view Exchange = "SPB";
    static constexpr std::string_view Venue = Decoder::name();
public:
    SpbProtocol(core::Executor& executor)
    : executor_(executor)
    , decoder_(streams())
    , bestprice_(*this)
    , instruments_(*this) {}

    SpbProtocol(const SpbProtocol&) = delete;
    SpbProtocol(SpbProtocol&&) = delete;
    
    constexpr std::string_view name() {  return Decoder::name(); }
    
    StreamsTuple streams() { return StreamsTuple(bestprice_, instruments_); }

    Decoder& decoder() { return decoder_; }
    auto& stats() {return decoder().stats(); }
    
    void on_packet(const BinaryPacket& e) { 
        decoder_.on_packet(e); 
    }
    
    core::Executor& executor() { return executor_; }

    void on_parameters_updated(const core::Parameters& params) {
        for(auto e: params) {
            auto strm = e.value_or("stream", std::string{});
            StreamsTuple strms = streams();
            mp::tuple_for_each(strms, [&](auto &s) {
                if(s.name() == strm) {
                    s.on_parameters_updated(e);
                }
            });
        }
    }
    std::string_view exchange() { return Exchange; }
    std::string_view venue() { return Venue; }
    PriceConv price_conv() { return PriceConv();}

    BestPriceStream& bestprice() { return bestprice_;}
    InstrumentStream& instruments() { return instruments_; }

    core::TickStream& ticks(core::StreamType streamtype) {
        return bestprice();
    }
    core::VenueInstrumentStream& instruments(core::StreamType streamtype) {
        return  instruments();
    }
private:
    core::Executor& executor_;
    Decoder decoder_;    
    BestPriceStream bestprice_;
    InstrumentStream instruments_;
};

template<typename BinaryPacketT>
using SpbUdpProtocol = SpbProtocol<SpbSchema<MdHeader>, BinaryPacketT>;

}