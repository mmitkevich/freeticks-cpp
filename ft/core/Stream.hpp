#pragma once

#include "ft/utils/Common.hpp"
#include "ft/core/Component.hpp"
#include "toolbox/util/Slot.hpp"
#include "ft/core/StreamStats.hpp"
#include "ft/capi/ft-types.h"

namespace ft::core {

/// Stream of events could be viewed as tb::Signal<const T&> from subscriber point of view
/// and tb::Slot<const T&> (or tb::Slot<T&&>) from publisher point of view
/// so, it has to have connect()/disconnect() methods and invoke()/empty()/operator()

template<typename SequenceId>
class Sequenced {
public:
    SequenceId sequence() const { return sequence_; }
    void sequence(SequenceId val) { sequence_ = val; }
protected:
    SequenceId sequence_{};
};


enum class StreamState : int8_t {
    Closed,
    Pending,
    Open,
    Stale,
    Closing,
    Failed
};

class StreamBase :  public BasicComponent<core::StreamState>, public Sequenced<std::uint64_t> 
{
public:
    core::StreamStats& stats() { return stats_; }
    void open() {}
    void close() {}
protected:
  core::StreamStats stats_;

};

/// (Input)Stream = Sequenced Signal
template<typename ...ArgsT>
class Stream : public tb::Signal<ArgsT...>, public StreamBase
{
    using Base = tb::Signal<ArgsT...>;
    using Sequenced = Sequenced<std::uint64_t>;
public:
    using Base::Base;
    using Base::connect;
    using Base::disconnect;
    using Sequenced::sequence;
};

/// (Output) Sink is Sequenced Slot
template<typename... ArgsT>
class Sink: public tb::Slot<ArgsT...>, public StreamBase {
    using Base = tb::Slot<ArgsT...>;
    using Sequenced = Sequenced<std::uint64_t>;
public:
    Sink(Base&& slot): Base(slot) {}
    using Base::Base;
    using Base::invoke;
    using Base::operator();
    using Sequenced::sequence;
};

enum class StreamType {
    Invalid = 0,
    Tick = FT_TICK,
    OHLC = FT_OHLC,
    Instrument = FT_INSTRUMENT
};

using StreamName = const char*;

inline std::ostream& operator<<(std::ostream& os, const StreamType self) {
    switch(self) {
        case StreamType::Invalid: return os << "Unknown";
        case StreamType::Tick: return os << "Tick";
        case StreamType::OHLC: return os << "OHLC";
        case StreamType::Instrument: return os << "Instrument";
        default: return os << (int)tb::unbox(self);
    }
}

class ProtocolBase {
public:
    void open() {}
    void close() {}
};

};