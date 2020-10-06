#pragma once
#include "ft/utils/Common.hpp"
#include "ft/utils/Ticks.hpp"

namespace ft::core {

struct TickMessage {
    using Price = std::int64_t;
    using Qty = std::int64_t;
 
    enum Type {
        Unknown = 0,
        Place  = 1<<0,
        Cancel = 1<<1,
        Move   = 0x03,
        Fill   = 1<<2
    };
    
    Type type() const {
        if(flags & Place)
            return Place;
        else if(flags & Cancel)
            return Cancel;
        else if(flags & Fill)
            return Fill;
        assert(false);
        return Unknown;
    }

    std::int64_t flags {};
    ftu::TimeStamp timestamp {}; 
    ftu::TimeStamp sender_timestamp {};
    ftu::TimeStamp exchange_timestamp {};
    std::int64_t client_id {0};
    std::int64_t exchange_id {0};
    Price price {0};
    Qty qty {0};
    Qty qty_left {0};
    Price fill_price {0};
    Qty fill_id {0};
    Qty open_interest {0};
};

inline std::ostream & operator<<(std::ostream& os, const TickMessage &e) {
    switch(e.type()) {
        case TickMessage::Place: os << "Place"; break;
        case TickMessage::Cancel: os << "Cancel"; break;
        case TickMessage::Fill: os << "Fill"; break;
        default: assert(false);
    }
    os << ", ";
    os << e.timestamp;
    os << ", ";
    os << e.price;
    os << ", ";
    os << e.qty;
    return os;
}

using TickSignal = tbu::Signal<const core::TickMessage&>;
using TickSlot = tbu::Signal<const core::TickMessage&>;

} // namespace