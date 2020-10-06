#pragma once

#include "ft/utils/Common.hpp"
#include "toolbox/sys/Time.hpp"

namespace ft::spb {

#pragma pack(push, 1)

using Int2 = std::uint16_t;
using Int4 = std::uint32_t;
using Int8 = std::uint64_t;
using Int1 = std::uint8_t;

struct Time8n {
    int64_t value = 0;
    friend std::ostream& operator<<(std::ostream& os, const Time8n& self) {
        os << toolbox::sys::put_time<toolbox::Nanos>(self.wall_time());
        return os;
    }
    toolbox::WallTime wall_time() const {
        return toolbox::WallTime(toolbox::WallTime::duration{value});
    }
};

struct Time8m {
    int64_t value = 0;
    friend std::ostream& operator<<(std::ostream& os, const Time8m& self) {
        os << self.wall_time();
        return os;
    }
    toolbox::WallTime wall_time() const {
        return toolbox::WallTime(toolbox::WallTime::duration{value*1000000});
    }
};

struct Time4 {
    uint32_t value = 0;
};

struct Dec8 {
    std::int64_t value;
    friend std::ostream& operator<<(std::ostream& os, const Dec8& self) {
        os << self.value/1e8;
        return os;
    }
};

using Size = Int2;
using MsgId = Int2;
using Seq = Int8;
using MarketId = Int2;
using InstrumentId = Int4;

struct Frame {
    Size size;
    MsgId msgid;
    Seq seq;

    Frame(MsgId msgid)
    : msgid(msgid) {}
    
    friend std::ostream& operator<<(std::ostream& os, const Frame& self) {
        os << "msgid "<<self.msgid<<" seq "<<self.seq;
        return os;
    }
};
static_assert(sizeof(Frame)==12);

struct Instrument {
    MarketId marketid;
    InstrumentId insturmentid;
    friend std::ostream& operator<<(std::ostream& os, const Instrument& self) {
        os << self.insturmentid << "_" << self.marketid;
        return os;
    }
};
static_assert(sizeof(Instrument)==6);

struct MdHeader {
    Time8n system_time;
    Int2 sourceid;
};

static_assert(sizeof(MdHeader)==10);

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
#pragma pack(pop)

} // ns
