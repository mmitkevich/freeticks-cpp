#pragma once
#include "ft/core/Instrument.hpp"
#include "ft/core/Stream.hpp"
#include "ft/utils/Common.hpp"
#include "toolbox/net/Endpoint.hpp"
#include <bitset>
#include <vector>

namespace ft { inline  namespace core {

using StreamSet = std::bitset<128>;

class Subscriber {
public:
    Subscriber() = default;
    Subscriber(InstrumentId instrument, StreamSet sset)
    : instrument_(instrument)
    , sset_(sset) {}
    bool test(StreamTopic topic) const {
        return sset_.test(tb::unbox(topic));
    }
    void set(StreamTopic topic) {
        sset_.set(tb::unbox(topic));
    }
private:
  InstrumentId instrument_;
  StreamSet sset_;
};

}} // ft::core