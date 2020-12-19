#pragma once

#include "ft/utils/Common.hpp"
#include "ft/core/Component.hpp"
#include "toolbox/util/Slot.hpp"
#include "ft/core/StreamStats.hpp"
#include "ft/capi/ft-types.h"

namespace ft { inline namespace core {

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
    Closed  = 0,
    Pending = 1,
    Open    = 2,
    Stale   = 3,
    Closing = 4,
    Failed  = 5
};

class StreamBase :  public BasicState<core::StreamState>, public Sequenced<std::uint64_t> 
{
public:
    StreamBase() = default;

    StreamBase(const StreamBase&) = delete;
    StreamBase& operator=(const StreamBase&) = delete;

    StreamBase(StreamBase&&) = delete;
    StreamBase& operator=(StreamBase&&) = delete;

    core::StreamStats& stats() { return stats_; }
protected:
  core::StreamStats stats_;
};

/// (Input)Stream = Sequenced Signal
template<typename ...ArgsT>
class Stream : public StreamBase, public tb::Signal<ArgsT...>
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
template<typename MessageT>
class Sink: public StreamBase, public tb::Slot<const MessageT&, tb::SizeSlot> {
    using Base = tb::Slot<const MessageT&, tb::SizeSlot>;
    using Slot = Base;
    using Sequenced = Sequenced<std::uint64_t>;
public:
    using Base::Base;
    Sink(Slot slot)
    : Base(slot) {}
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
    ProtocolBase() = default;
    // Copy
    ProtocolBase(const ProtocolBase&) = delete;
    ProtocolBase&operator=(const ProtocolBase&) = delete;

    // Move
    ProtocolBase(ProtocolBase&&) = default;
    ProtocolBase& operator=(ProtocolBase&&) = default;

    void open() {}
    void close() {}
};

}} // ft::core