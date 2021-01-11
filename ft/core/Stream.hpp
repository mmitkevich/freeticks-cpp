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


enum class StreamTopic: ft_topic_t {
    Empty = 0,
    BestPrice = FT_BESTPRICE,
    Instrument = FT_INSTRUMENT,
    Candle = FT_CANDLE,
};

enum class Event : ft_event_t {
    Empty = 0,
    Snapshot = FT_SNAPSHOT,
    Update = FT_UPDATE
};

inline std::ostream& operator<<(std::ostream& os, const Event self) {
    switch(self) {
        case Event::Empty: return os << "Empty";
        case Event::Snapshot: return os << "Snapshot";
        case Event::Update: return os << "Update";
        default: return os << (int)tb::unbox(self);
    }
}

inline std::ostream& operator<<(std::ostream& os, const StreamTopic self) {
    switch(self) {
        case StreamTopic::Empty: return os << "Empty";
        case StreamTopic::BestPrice: return os << "BestPrice";
        case StreamTopic::Candle: return os << "Candle";
        case StreamTopic::Instrument: return os << "Instrument";
        default: return os << (int)tb::unbox(self);
    }
}

class StreamBase :  public BasicStates<core::StreamState>, public Sequenced<std::uint64_t> 
{
public:
    StreamBase() = default;

    StreamBase(const StreamBase&) = delete;
    StreamBase& operator=(const StreamBase&) = delete;

    StreamBase(StreamBase&&) = delete;
    StreamBase& operator=(StreamBase&&) = delete;

    core::StreamStats& stats() { return stats_; }

    StreamTopic topic() const { return topic_; }
    void topic(StreamTopic topic) { topic_ = topic; }
protected:
    core::StreamStats stats_;
    StreamTopic topic_{StreamTopic::Empty};
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
    using Base::operator();
    using Base::invoke;
};

}} // ft::core