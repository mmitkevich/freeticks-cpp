#pragma once
#include "boost/mp11/bind.hpp"
#include "ft/core/Component.hpp"
#include "ft/core/Instrument.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/SequencedChannel.hpp"
#include "ft/core/Tick.hpp"
#include "ft/io/Conn.hpp"
#include "ft/spb/SpbFrame.hpp"
#include "ft/utils/Common.hpp"
#include "SpbSchema.hpp"
#include "SpbDecoder.hpp"
#include "SpbBestPriceStream.hpp"
#include "SpbInstrumentStream.hpp"
#include "toolbox/util/Slot.hpp"
#include "toolbox/util/Tuple.hpp"
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <cstdint>
#include <sstream>
#include <stdexcept>
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

template<class Self, class SchemaT, typename...O>
class BasicSpbProtocol : public io::BasicMdProtocol<Self, O...>
{
    FT_SELF(Self)
protected:
    using Base = io::BasicMdProtocol<Self, O...>;
public:
    using Schema = SchemaT;
    using BestPriceSignal = SpbBestPriceStream<Schema>;
    using InstrumentSignal = SpbInstrumentStream<Schema>;
    
    using StreamsTuple = std::tuple<BestPriceSignal*, InstrumentSignal*>;
    using Decoder = SpbDecoder<spb::Frame, SchemaT, StreamsTuple>;

public:
    BasicSpbProtocol()
    : decoder_(self()->streams()) {} // FIXME: streams() crashed?

    constexpr std::string_view name() {  return Decoder::name(); }
    
    StreamsTuple streams() { return StreamsTuple(&self()->bestprice(), &self()->instruments()); }

    Decoder& decoder() { return decoder_; }
    auto& stats() {return decoder().stats(); }
    
    /// use decoder
    template<class ConnT, class BinaryPacketT>
    void async_handle(ConnT& conn, const BinaryPacketT& packet, tb::DoneSlot done) { 
        decoder_(packet); 
        done({});
    }

    void open() {
        self()->bestprice().open();
        self()->instruments().open();
    }
    auto& bestprice() { return bestprice_signal_; }
    auto& instruments() { return instruments_signal_; }

protected:
    void on_parameters_updated(const core::Parameters& params) {
        for(auto e: params["connections"]) {
            auto strm = e.str("topic", "");
            mp::tuple_for_each(streams(), [&](auto* s) {
                if(s->name() == strm) {
                    s->on_parameters_updated(e);
                }
            });
        }
    }
protected:
    // from server to client
    BestPriceSignal bestprice_signal_;
    InstrumentSignal instruments_signal_;

    Decoder decoder_;    
};

template<class EndpointT>
struct SpbProtocol {
    template<class Self, typename...O>
    using Mixin = BasicSpbProtocol<Self, SpbSchema<spb::MdHeader, EndpointT>, O...>;
};
}