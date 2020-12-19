#pragma once
#include "ft/core/Instrument.hpp"
#include "ft/core/Stream.hpp"
#include "ft/utils/Common.hpp"
#include "toolbox/net/Endpoint.hpp"
#include <bitset>
#include <vector>

namespace ft { inline  namespace core {

using StreamSet = std::bitset<128>;

class MdSub {
public:
    MdSub() = default;
    MdSub(InstrumentId instrument, StreamSet sset)
    : instrument_(instrument)
    , sset_(sset) {}
    bool test(StreamType stream) {
        return sset_.test(tb::unbox(stream));
    }
    void set(StreamType stream) {
        sset_.set(tb::unbox(stream));
    }
private:
  InstrumentId instrument_;
  StreamSet sset_;
};

}} // ft::core