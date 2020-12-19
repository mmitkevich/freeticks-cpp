#pragma once
#include "../utils/Common.hpp"
#include "ft/core/Instrument.hpp"
#include "ft/core/StreamStats.hpp"
#include "toolbox/util/Slot.hpp"
#include "toolbox/net/Packet.hpp"
#include <csignal>
#include <ctime>
#include <fstream>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <iomanip>
#include <string_view>
#include "ft/utils/TimeUtils.hpp"
#include "ft/core/Tick.hpp"
#include "toolbox/util.hpp"

namespace ft::qsh {
    
using QshTick = ft::core::Ticks<2>;
using Timestamp = ft::core::Timestamp;

class QshDecoder
{
public:
    enum Stream {
        Quotes          = 0x10,
        Deals           = 0x20,
        OwnOrders       = 0x30,
        OwnTrades       = 0x40,
        Messages        = 0x50,
        AuxInfo         = 0x60,
        OrdLog          = 0x70
    };

    enum PlazaFlags {
        PLAZA_ADD = 1<<2,
        PLAZA_FILL = 1<<3,
        PLAZA_BUY = 1<<4,
        PLAZA_SELL = 1<<5,
        PLAZA_SNAPSHOT = 1<<6,
        PLAZA_QUOTE = 1<<7,
        PLAZA_COUNTER = 1<<8,
        PLAZA_NONSYSTEM = 1<<9,
        PLAZA_END_OF_TX = 1<<10,
        PLAZA_FOC = 1<<11,
        PLAZA_MOVE = 1<<12,
        PLAZA_CANCEL = 1<<13,
        PLAZA_CANCEL_GROUP = 1<<14,
        PLAZA_CROSS = 1<<15
    };

    enum OrdLogFlags {
        OL_TIMESTAMP = 1<<0,
        OL_ID = 1<<1,
        OL_PRICE = 1<<2,
        OL_AMOUNT = 1<<3,
        OL_AMOUNT_LEFT = 1<<4,
        OL_FILL_ID = 1<<5,
        OL_FILL_PRICE = 1<<6,
        OL_OPEN_INTEREST = 1<<7
    };

    using Qty = ft::core::Qty;
    using Price = ft::core::Price;
    using ExchangeId = ft::core::ExchangeId;
    
    struct State {
        std::string app;
        std::string comment;
        ft::HundredNanos ctime {0};
        int nstreams {0};
        std::size_t cur_stream {0};
        std::array<std::string, 7> instruments;
        std::array<int, 7> stream_ids;
        ft::HundredNanos frame_ts {0};
        ft::HundredNanos ts {0};
        Price price {0};
        ExchangeId fill_id {0};
        Price fill_price {0};
        Qty open_interest {0};
        ExchangeId exchange_id;
    };

public:
    void input(std::istream &is) {
        input_stream_ = &is;
    }
    std::istream& input_stream(){ return *input_stream_;}

    core::TickStream& ticks() { return ticks_; }
    core::InstrumentStream& instruments() { return instruments_; }
    
    /// decode stream until EOF
    void run();
private:
    void read_order_log();
    void read_header();
    bool read_frame_header();
    std::string read_string();
    std::int64_t read_leb128();
    std::uint64_t read_uleb128();
    std::uint64_t read_growing(std::uint64_t previous);
    std::uint64_t read_relative(std::uint64_t previous);
    ft::HundredNanos read_datetime();
    ft::HundredNanos read_grow_datetime(ft::HundredNanos previous);
    int read_byte();
    std::uint16_t read_uint16();
private:
    State state_;
    std::istream* input_stream_;
    core::TickStream ticks_;
    core::InstrumentStream instruments_;
};

}