#pragma once
#include <cstdint>
#include <stdexcept>
#include "ft/core/Requests.hpp"
#include "ft/sbe/SbeTypes.hpp"

namespace ft::tbricks::v1 {

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
    String symbol;
    Sequence seq;
    std::size_t length() const { return symbol.length()+sizeof(seq); }
};
struct SequencePayload {
    Sequence seq;
    std::size_t length() const { return sizeof(seq); }
};
struct MarketDataPayload {
    String symbol;
    Price bid;
    Price ask;
    Phase phase;
    MDStatus status;
    Timestamp time;
    Sequence seq;
    
    std::size_t length() const {
        return symbol.length()
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
    
    Message(MessageType msgtype = MessageType::NotAMessage)
    : msgtype(msgtype)
    {}

    std::size_t length() const { 
        switch(msgtype) {
            case MessageType::SubscriptionRequest: 
            case MessageType::SubscriptionRequestBBO:
            case MessageType::SubscriptionCancelRequest:
                return sizeof(msgtype) + subscription.length();
            case MessageType::MarketData:
            case MessageType::MarketDataBBO:
                return sizeof(msgtype) + marketdata.length();
            case MessageType::HeartBeat:
            case MessageType::ClosingEvent:            
                return sizeof(msgtype) + sequence.length();
            default: 
                assert(false);
                return sizeof(msgtype);
        }
    }
    bool is_valid() {
        switch(msgtype) {
            case MessageType::SubscriptionRequest:
            case MessageType::SubscriptionCancelRequest: 
            case MessageType::MarketData: 
            case MessageType::ClosingEvent: 
            case MessageType::HeartBeat:
                return true;
            default: return false;
        }
    }
    Sequence seq() const {
        switch(msgtype) {
            case MessageType::SubscriptionRequest: 
            case MessageType::SubscriptionCancelRequest: 
                return subscription.seq;
            case MessageType::MarketData:
                return marketdata.seq;
            case MessageType::ClosingEvent: 
            case MessageType::HeartBeat:
                return sequence.seq;
            default: throw std::runtime_error("bad_msgtype");
        }
    }
    void seq(Sequence seq) {
        switch(msgtype) {
            case MessageType::SubscriptionRequest: 
            case MessageType::SubscriptionRequestBBO:
            case MessageType::SubscriptionCancelRequest: 
                subscription.seq = seq;
                break;
            case MessageType::MarketData: 
                marketdata.seq = seq;
                break;
            case MessageType::ClosingEvent:
            case MessageType::HeartBeat:
                sequence.seq = seq;
                break;
            default: throw std::runtime_error("bad_msgtype");
        }
    }
    void symbol(std::string_view val) {
        switch(msgtype) {
            case MessageType::SubscriptionRequest: 
            case MessageType::SubscriptionRequestBBO:
            case MessageType::SubscriptionCancelRequest:
                subscription.symbol = val;
                break;
            default:
                assert(false);
        }
    }
    std::string_view symbol() const {
        switch(msgtype) {
            case MessageType::SubscriptionRequest: 
            case MessageType::SubscriptionRequestBBO:
            case MessageType::SubscriptionCancelRequest:
                return subscription.symbol.str();
            default:
                assert(false);
                return {};
        }
    }
};

};