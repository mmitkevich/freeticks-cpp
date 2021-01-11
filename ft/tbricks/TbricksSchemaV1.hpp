#pragma once
#include <boost/type_traits/is_detected.hpp>
#include <cstdint>
#include <stdexcept>
#include "ft/core/Requests.hpp"
#include "ft/sbe/SbeTypes.hpp"

namespace ft::tbricks::v1 {

#pragma pack(push, 1)

enum class MessageType: uint8_t {
    NotAMessage, 
    SubscriptionRequest, 
    SubscriptionCancelRequest, 
    ClosingEvent, 
    MarketData, 
    HeartBeat,
    SubscriptionRequestBBO,
    MarketDataBBO
};

enum  class Phase:  uint8_t {NotSet, Trading, Closed, Halted, Auction};
enum  class MDStatus:  uint8_t {NotSet, Failed, Pending, Staled, OK};

using String = sbe::BasicVarString<std::size_t>;
using Sequence = std::uint64_t;

struct Price {
    double value;
    bool empty;
};

struct Timestamp {
    std::uint64_t value;
};

struct SubscriptionPayload {
#define TB_PADVAL 0
    TB_FIELD(String, symbol, symbol_);
#undef  TB_PADVAL
#define TB_PADVAL TB_PADSIZE(symbol_)
    TB_FIELD(Sequence, seq, seq_);
    TB_BYTESIZE(*this)
#undef TB_PADVAL
};

struct SequencePayload {
#define TB_PADVAL 0
    TB_FIELD(Sequence, seq, seq_);
    TB_BYTESIZE(*this)
#undef TB_PADVAL
};

struct MarketDataPayload {
#define TB_PADVAL 0
    TB_FIELD(String, symbol, symbol_);
#undef TB_PADVAL
#define TB_PADVAL TB_PADSIZE(symbol_)
    TB_FIELD(Price, bid, bid_);
    TB_FIELD(Price, ask, ask_);
    TB_FIELD(Phase, phase, phase_);
    TB_FIELD(MDStatus, status, status_);
    TB_FIELD(Timestamp, time, time_);
    TB_FIELD(Sequence, seq, seq_);
    TB_BYTESIZE(*this)
#undef TB_PADVAL
};

template<std::size_t PadSizeI=0>
struct Message {
    MessageType msgtype_;
    union {
        SubscriptionPayload subscription;
        MarketDataPayload marketdata;
        SequencePayload sequence;
    };
    char data_[PadSizeI];

    auto& msgtype() { return msgtype_; }
    const auto& msgtype() const { return msgtype_; }


    Message(MessageType msgtype = MessageType::NotAMessage)
    : msgtype_(msgtype)
    {}

    template<std::size_t NewSizeI>
    Message<NewSizeI>& as_size() {
        return *reinterpret_cast<Message<NewSizeI>*>(this);
    }

    std::size_t bytesize() const { 
        switch(msgtype()) {
            case MessageType::SubscriptionRequest: 
            case MessageType::SubscriptionRequestBBO:
            case MessageType::SubscriptionCancelRequest:
                return sizeof(msgtype()) + subscription.bytesize();
            case MessageType::MarketData:
            case MessageType::MarketDataBBO:
                return sizeof(msgtype()) + marketdata.bytesize();
            case MessageType::HeartBeat:
            case MessageType::ClosingEvent:            
                return sizeof(msgtype()) + sequence.bytesize();
            default: 
                assert(false);
                return sizeof(msgtype_);
        }
    }
    bool is_valid() {
        switch(msgtype_) {
            case MessageType::SubscriptionRequest:
            case MessageType::SubscriptionCancelRequest: 
            case MessageType::MarketData: 
            case MessageType::ClosingEvent: 
            case MessageType::HeartBeat:
                return true;
            default: return false;
        }
    }

    TB_FIELD_FN(Sequence, seq, {
        switch(msgtype_) {
            case MessageType::SubscriptionRequest: 
            case MessageType::SubscriptionCancelRequest: 
                return subscription.seq();
            case MessageType::MarketData:
                return marketdata.seq();
            case MessageType::ClosingEvent: 
            case MessageType::HeartBeat:
                return sequence.seq();
            default: throw std::runtime_error("bad_msgtype");
        }
    });

    TB_FIELD_FN(String, symbol, {
        switch(msgtype_) {
            case MessageType::SubscriptionRequest: 
            case MessageType::SubscriptionRequestBBO:
            case MessageType::SubscriptionCancelRequest:
                return subscription.symbol();
                break;
            default:
                throw std::runtime_error("bad_msgtype");
        }
    });
};

#pragma pack(pop)
} // tbricks::schema::v1