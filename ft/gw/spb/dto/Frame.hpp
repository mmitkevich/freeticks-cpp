#include <cstdint>
#include <array>

namespace ft::gw::spb::dto {

using Int2 = std::uint16_t;
using Int4 = std::uint32_t;
using Int8 = std::uint64_t;
using Int1 = std::uint8_t;

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

#pragma pack(push, 1)
struct Frame {
    Size size;
    MsgId msgid;
    Seq seq;

    Frame(MsgId msgid)
    : msgid(msgid) {}
};
#pragma pack(pop)

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
