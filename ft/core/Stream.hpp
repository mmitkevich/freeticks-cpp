#pragma once

#include "ft/utils/Common.hpp"
#include "ft/core/Component.hpp"
#include "toolbox/util/Enum.hpp"
#include "toolbox/util/Slot.hpp"
#include "ft/core/StreamStats.hpp"
#include "ft/capi/ft-types.h"
#include <boost/core/noncopyable.hpp>
#include <unordered_map>

namespace ft { inline namespace core {

/// Stream of events could be viewed as tb::Signal<const T&> from subscriber point of view
/// and tb::Slot<const T&> (or tb::Slot<T&&>) from publisher point of view
/// so, it has to have connect()/disconnect() methods and invoke()/empty()/operator()

template<typename SequenceId>
class BasicSequenced {
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
    Empty = FT_TOPIC_EMPTY,
    BestPrice = FT_TOPIC_BESTPRICE,
    Instrument = FT_TOPIC_INSTRUMENT,
    Candle = FT_TOPIC_CANDLE,
};


inline constexpr StreamTopic topic_from_name(std::string_view s) {
    if(s=="BestPrice") {
        return StreamTopic::BestPrice;
    } else if(s=="Instrument") {
        return StreamTopic::Instrument;
    } else  {
        return StreamTopic::Empty;
    }
}
inline constexpr const char* topic_to_name(const StreamTopic topic) {
    switch(topic) {
        case StreamTopic::BestPrice: return "BestPrice";
        case StreamTopic::Instrument: return "Instrument";
        case StreamTopic::Candle: return "Candle";
        case StreamTopic::Empty: return "Empty";
        default: return "Invalid";
    }
}

inline std::ostream& operator<<(std::ostream& os, const StreamTopic self) {
    switch(self) {
        case StreamTopic::BestPrice:
        case StreamTopic::Instrument:
        case StreamTopic::Candle:
        case StreamTopic::Empty:
            return os << topic_to_name(self);
        default:
            return os << tb::unbox(self);
    }
}

enum class Event : ft_event_t {
    Empty = FT_EVENT_EMPTY,
    Snapshot = FT_EVENT_SNAPSHOT,
    Update = FT_EVENT_UPDATE
};

inline std::ostream& operator<<(std::ostream& os, const Event self) {
    switch(self) {
        case Event::Empty: return os << "Empty";
        case Event::Snapshot: return os << "Snapshot";
        case Event::Update: return os << "Update";
        default: return os << (int)tb::unbox(self);
    }
}


using StreamSet = tb::BitMask<StreamTopic>;

class Subscription: public tb::BitMask<StreamTopic> {
    using Base = tb::BitMask<StreamTopic>;
public:
    Subscription() = default;
    
    bool test(StreamTopic topic, InstrumentId instrument) {
        if(!Base::test(topic))
            return false;
        auto it = instruments_.find(instrument);
        if(it==instruments_.end())
            return false;
        return it->second.test(topic);
    }
    Subscription& subscribe(StreamTopic topic, InstrumentId instrument) {
        instruments_[instrument].set(topic);
        Base::set(topic);
        return *this;
    }
    Subscription& unsubscribe(StreamTopic topic, InstrumentId instrument) {
        instruments_[instrument].reset(topic);
        return *this;
    }
protected:
    tb::unordered_map<InstrumentId, tb::BitMask<StreamTopic>> instruments_;
};


class Stream
: public core::BasicStateful<core::StreamState>
, public core::BasicSequenced<std::uint64_t>
, public core::Movable
{
    using Sequenced = core::BasicSequenced<std::uint64_t>;
    using Stateful = core::BasicStateful<core::StreamState>;
public:
    Stream() = default;
    Stream(StreamTopic topic) {
        topic_= topic;
    }
    using Sequenced::sequence;
    using Stateful::state;

    core::StreamStats& stats() { return stats_; }

    constexpr StreamTopic topic() const { return topic_; }
    void topic(StreamTopic topic) { topic_ = topic; }

    constexpr std::string_view name() { return topic_to_name(topic()); }

    template<typename...ArgsT>
    class Signal; /// type erased signal

    template<typename...ArgsT>
    class Slot;  /// type erased sink

    template<class Impl, typename...ArgsT>
    class BasicSlot; /// typed sink (fast)

    template<typename...ArgsT>
    Signal<ArgsT...>& signal();
    template<typename...ArgsT>
    Slot<ArgsT...>& slot();
protected:
    StreamTopic topic_{StreamTopic::Empty};
    core::StreamStats stats_;
};

/// (Input) Stream = Sequenced Signal
template<typename ...ArgsT>
class Stream::Signal : public tb::Signal<ArgsT...>, public Stream
{
    using Base = tb::Signal<ArgsT...>;
public:
    using Base::Base;
    using Base::connect, Base::disconnect;
    using Stream::sequence, Stream::topic;
    using Stream::Stream;
    
    static Stream::Signal<ArgsT...>& from(Stream& strm) {
        // FIXME: type check?
        return static_cast<Stream::Signal<ArgsT...>&>(strm);
    }
};


template<typename...ArgsT>
Stream::Signal<ArgsT...>& Stream::signal() {
    return Stream::Signal<ArgsT...>::from(this);
}


/// Output stream (type erased)
template<typename...ArgsT>
class Stream::Slot : public tb::Slot<ArgsT...>,  public Stream  {
    using Base = tb::Slot<ArgsT...>;
public:
    using Base::Base;
    using Base::connect, Base::disconnect;
    using Stream::sequence, Stream::topic;
    
    static Slot& from(Stream& strm) {
        // FIXME: type check?
        return static_cast<Slot&>(strm);
    }
};

template<typename...ArgsT>
Stream::Slot<ArgsT...>& Stream::slot() {
    return Stream::Slot<ArgsT...>::from(this);
}
/// Output Stream (faster if type is known)
template<class Self, typename...ArgsT>
class Stream::BasicSlot: public Stream::Slot<ArgsT...> {
    using Base = Stream::Slot<ArgsT...>;
public:
    FT_SELF(Self);
    using Stream::sequence, Stream::topic;
    
    /// binds pointer to derived Self 
    BasicSlot() : Base(tb::bind<&Self::invoke>(self())) {}

    void operator()(ArgsT... args) {
        self()->invoke(std::forward<ArgsT>(args)...);
    }

    static Self& from(Stream& strm) {
        // FIXME: type check?
        return static_cast<Self&>(strm);
    }
};
}} // ft::core
