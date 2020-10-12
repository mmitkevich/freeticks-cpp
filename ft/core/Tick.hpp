#pragma once
#include "ft/utils/Common.hpp"
#include "ft/utils/TimeUtils.hpp"
#include "ft/core/Stream.hpp"
#include "toolbox/sys/Time.hpp"
#include "toolbox/util/Slot.hpp"
#include <cstdint>

namespace ft::core {

using Timestamp = toolbox::WallTime;
using Price = std::int64_t;
using Qty = std::int64_t;
using ClientId = std::int64_t;
using ExchangeId = std::int64_t;

struct DefaultPolicy {
    using ExchangeId = ft::core::ExchangeId;
    using ClientId = ft::core::ClientId;
    using Price = ft::core::Price;
    using Qty = ft::core::Qty;
    using Timestamp = ft::core::Timestamp;
};

enum class TickType : std::uint8_t {
    Unknown = 0,
    Place   = 1,
    Cancel  = 2,
    Move    = 3,
    Fill    = 4
};

template<typename PolicyT>
struct BasicTick {
public:
    using Price = typename PolicyT::Price;
    using Qty = typename PolicyT::Qty;
    using ClientId = typename PolicyT::ClientId;
    using ExchangeId = typename PolicyT::ExchangeId;
    using Timestamp = typename PolicyT::Timestamp;

public:    
    TickType type() const {
        using namespace tbu::operators;
        if(flags & TickType::Place)
            return TickType::Place;
        else if(flags & TickType::Cancel)
            return TickType::Cancel;
        else if(flags & TickType::Fill)
            return TickType:: Fill;
        assert(false);
        return TickType::Unknown;
    }
public:
    std::uint64_t flags {};
    Timestamp timestamp {}; 
    Timestamp sender_timestamp {};
    Timestamp exchange_timestamp {};
    ClientId client_id {};
    ExchangeId exchange_id {};
    Price price {};
    Qty qty {};
    Qty qty_left {};
    Price fill_price {};
    Qty fill_id {};
    Qty open_interest {};
};

using Tick = BasicTick<DefaultPolicy>;
using TickStream = core::Stream<Tick>;
using TickSignal = tbu::Signal<Tick>;
using TickSlot = TickSignal::Slot;

template<typename PolicyT>
inline std::ostream & operator<<(std::ostream& os, const BasicTick<PolicyT> &e) {
    os<<"type:";
    switch(e.type()) {
        case TickType::Place: os << "Place"; break;
        case TickType::Cancel: os << "Cancel"; break;
        case TickType::Fill: os << "Fill"; break;
        default: assert(false);
    }
    os << ",time:";
    os << e.timestamp;
    os << ",price:";
    os << e.price;
    os << ",qty:";
    os << e.qty;
    return os;
}

} // namespace