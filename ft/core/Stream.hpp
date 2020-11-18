#pragma once

#include "ft/utils/Common.hpp"
#include "toolbox/util/Slot.hpp"
#include "ft/core/StreamStats.hpp"
#include "ft/capi/ft-types.h"

namespace ft::core {


template<typename SequenceId>
class Sequenced {
public:
    SequenceId sequence() const { return sequence_; }
    void sequence(SequenceId val) { sequence_ = val; }
protected:
    SequenceId sequence_{};
};


template<typename MessageT>
class Stream : public tb::Signal<const MessageT&>, public Sequenced<std::uint64_t>
{
    using Base = tb::Signal<const MessageT&>;
    using Sequenced = Sequenced<std::uint64_t>;
public:
    using Base::Base;
    using Base::connect;
    using Base::disconnect;
    using Sequenced::sequence;

    core::StreamStats& stats() { return stats_; }
protected:
  core::StreamStats stats_;
};

enum class StreamType {
    Invalid = 0,
    Tick = FT_TICK,
    OHLC = FT_OHLC,
    Instrument = FT_INSTRUMENT
};

inline std::ostream& operator<<(std::ostream& os, const StreamType self) {
    switch(self) {
        case StreamType::Invalid: return os << "Unknown";
        case StreamType::Tick: return os << "Tick";
        case StreamType::OHLC: return os << "OHLC";
        case StreamType::Instrument: return os << "Instrument";
        default: return os << (int)tb::unbox(self);
    }
}

};