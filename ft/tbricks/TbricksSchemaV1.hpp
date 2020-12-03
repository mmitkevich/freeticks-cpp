#pragma once
#include <cstdint>
#include <stdexcept>
#include "ft/sbe/SbeTypes.hpp"

namespace ft::tbricks::v1 {

enum class MessageType: uint8_t {
    NotAMessage, 
    SubscriptionRequest, 
    SubscriptionCancelRequest, 
    ClosingEvent, 
    MarketData, 
    HeartBeat
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
    String symbol;
    Sequence seq;
};
struct SequencePayload {
    Sequence seq;
};
struct MarketDataPayload {
    String symbol;
    Price bid;
    Price ask;
    Phase phase;
    MDStatus status;
    Timestamp time;
    Sequence seq;
    
    std::size_t size() const {
        return symbol.size()
            + sizeof(bid)
            + sizeof(ask)
            + sizeof(phase)
            + sizeof(status)
            + sizeof(time)
            + sizeof(seq);
    }
};

struct Message {
    MessageType msgtype;
    union {
        SubscriptionPayload subscription;
        MarketDataPayload marketdata;
        SequencePayload sequence;
    };
    bool is_valid() {
        switch(msgtype) {
            case MessageType::SubscriptionRequest:
            case MessageType::SubscriptionCancelRequest: 
            case MessageType::MarketData: 
            case MessageType::ClosingEvent: 
                return true;
            default: return false;
        }
    }
    Sequence seq() const {
        switch(msgtype) {
            case MessageType::SubscriptionRequest: 
            case MessageType::SubscriptionCancelRequest: 
                return subscription.seq;
            case MessageType::MarketData: return marketdata.seq;
            case MessageType::ClosingEvent: return sequence.seq;
            default: throw std::runtime_error("bad_msgtype");
        }
    }
    void seq(Sequence seq) {
        switch(msgtype) {
            case MessageType::SubscriptionRequest: 
            case MessageType::SubscriptionCancelRequest: 
                subscription.seq = seq;
            case MessageType::MarketData: marketdata.seq = seq;
            case MessageType::ClosingEvent: sequence.seq = seq;
            default: throw std::runtime_error("bad_msgtype");
        }
    }
    std::size_t size() const {
        return sizeof(msgtype) + MarketDataPayload().size();
    }
};

};