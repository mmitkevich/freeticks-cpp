#pragma once

#include <ctime>
#include <fstream>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <iomanip>
#include "TickMessage.hpp"
#include "util/Ticks.hpp"

class QScalp {
public:
    
    enum Stream {
        Quotes = 0x10,
        Deals = 0x20,
        OwnOrders = 0x30,
        OwnTrades = 0x40,
        Messages = 0x50,
        AuxInfo = 0x60,
        OrdLog = 0x70
    };

    using OnRecord = std::function<void(const TickMessage&)>;

    static void parse(std::string fname, OnRecord on_record);
    void read(std::istream &input, OnRecord on_record);
private:
    void read_ord_log(std::istream &input, TickMessage &e);
    static std::string read_string(std::istream& input);
    static std::int64_t read_leb128(std::istream& input);
    static std::int64_t read_uleb128(std::istream& input);
    static std::int64_t read_growing(std::istream& input, std::int64_t previous);
    static std::int64_t read_relative(std::istream &input, std::int64_t previous);
    static Ticks read_datetime(std::istream& input);
    static Ticks read_grow_datetime(std::istream& input, Ticks previous);
    static int read_byte(std::istream& input);
    static std::uint16_t read_uint16(std::istream& input);
    void read_header(std::istream& input);
    bool read_frame_header(std::istream& input);
public:
    using Qty = std::int64_t;
    using Price = std::int64_t;
    using Identifier = std::int64_t;
    std::string app;
    std::string comment;
    Ticks ctime {0};
    int nstreams {0};
    std::size_t cur_stream {0};
    std::array<std::string, 7> instruments;
    std::array<int, 7> stream_ids;
    Ticks frame_ts {0};
    Ticks ts {0};
    Price price {0};
    Identifier fill_id {0};
    Price fill_price {0};
    Qty open_interest {0};
    Identifier exchange_id;
};