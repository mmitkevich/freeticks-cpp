#include <cstdint>
#include <array>

namespace ft::gw::spb {

struct Int2 {
    uint16_t value = 0;
    
    constexpr Int2() {}
    constexpr Int2(uint16_t rhs)
    : value(rhs) {}
};

struct Int4 {
    uint32_t value = 0;
};

struct Int1 {
    uint8_t value = 0;
};

struct Int8 {
    uint64_t value = 0;
};

struct Time8n {
    uint64_t value = 0;
};

struct Time8m {
    uint64_t value = 0;
};

struct Time4 {
    uint32_t value = 0;
};

struct Dec8 {
    std::array<char, 8> value;
};

using Size = Int2;
using MsgId = Int2;
using Seq = Int8;
using MarketId = Int2;
using InstrumentId = Int8;

struct Frame {
    Size size;
    MsgId msgid;
    Seq seq;

    Frame(MsgId msgid)
    : msgid(msgid) {}
};

struct MarketInstrument {
    MarketId marketid;
    InstrumentId insturmentid;
};

struct MdHeader {
    Time8n system_time;
    Int2 sourceid;
};

using ClOrderId = std::array<char, 16>;
using UserId = std::array<char, 16>;

struct UserHeader {
    ClOrderId clorder_id;
};

struct GateHeader {
    Time8n system_time;
    Int2 source_id;
    ClOrderId clorder_id;
    UserId user_id;
};

struct Header {
    Int4 topic_id;
    Int8 topic_seq;
    Time8n system_time;
    Int2 source_id;
};
}
