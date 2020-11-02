#pragma once

#include "ft/core/Tick.hpp"
#include "ft/utils/Common.hpp"
#include "ft/utils/StringUtils.hpp"
#include "toolbox/sys/Time.hpp"
#include <cstring>
#include <string>
#include <string_view>

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
    operator std::int64_t() const { return value; }
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
    : msgid(msgid)
    , seq(0)
    , size(0) {}

    friend std::ostream& operator<<(std::ostream& os, const Frame& self) {
        os << "msgid:"<<self.msgid<<",seq:"<<self.seq;
        return os;
    }
};
static_assert(sizeof(Frame)==12);

struct MarketInstrumentId {
    MarketId market_id;
    InstrumentId instrument_id;
    friend std::ostream& operator<<(std::ostream& os, const MarketInstrumentId& self) {
        os << "["<<self.instrument_id << "," << self.market_id <<"]";
        return os;
    }
};
static_assert(sizeof(MarketInstrumentId)==6);

struct MdHeader {
    Time8n system_time;
    Int2 sourceid;

    friend std::ostream& operator<<(std::ostream& os, const MdHeader& self) {
        return os << "system_time:'" << self.system_time<<"'"<<",sourceid:"<<self.sourceid;
    }
};

static_assert(sizeof(MdHeader)==10);

template<std::size_t length>
class Characters {
    char value[length+1] {};
public:
    constexpr Characters() = default;
    constexpr Characters(const char* value) : value(value) {}
    constexpr std::size_t capacity() const { return length; }
    constexpr std::size_t size() const { return std::char_traits<char>::length(value);}
    constexpr std::string_view str() const { return std::string_view(value, size()); }
    std::wstring wstr() const { return ft::utils::to_wstring(str()); }
    constexpr const char* c_str() const { return value; }

    friend auto& operator<<(std::ostream& os, const Characters& self) {
        return os << self.str();
    }
};

using ClOrderId = std::array<char, 16>;
using UserId = std::array<char, 16>;

struct UserHeader {
    ClOrderId clorder_id;
};

using SourceId = Int2;

struct GateHeader {
    Time8n system_time;
    SourceId source_id;
    ClOrderId clorder_id;
    UserId user_id;
};

struct Header {
    Int4 topic_id;
    Int8 topic_seq;
    Time8n system_time;
    SourceId source_id;
};
#pragma pack(pop)

} // ns
