#pragma once

#include <boost/mp11/detail/mp_list.hpp>
#include <cassert>
#include <array>

#include "Frame.hpp"

#define SPB_ASSERT assert

namespace ft::gw::spb::dto {

struct Schema {
    struct UDP {
        using Header = MdHeader;
    };
};

template<uint16_t ID, typename SchemaT=Schema::UDP>
struct Message {
    static constexpr MsgId msgid {ID};
    Frame frame {ID};
    using Header = typename SchemaT::Header;
    Header header;
};

template<uint16_t ID, typename SchemaT=Schema::UDP>
struct Snapshot: public Message<ID> {
    Int8 update_seq;
};

template<typename SchemaT=Schema::UDP>
using SnapshotStart = Snapshot<12345, SchemaT>;

template<typename SchemaT=Schema::UDP>
using SnapshotFinish = Snapshot<12312, SchemaT>;

struct Side {
    enum Value:char {
        INVALID = 0,
        BUY_DIR = 1,
        SELL_DIR = 2,
        LAST_DEAL = 3
    };
    
    Value value = INVALID;

    Side() {}
    Side(Value side)
    : value(side) {}
};

struct NewFlag {
    enum Value:char {
        UPDATE = 0,
        NEW = 1
    };
    Value value = UPDATE;
};

using Amount = Int4;

struct SubAggr {
    Dec8 price;
    Side side;
    NewFlag flag;
    Amount amount;
    Time8n time;
};

template<typename ElementT>
struct Group {
    Int2 offset;
    Int2 count;

    std::size_t size() const {
        return count;
    }

    ElementT& operator[](std::size_t index) {
        SPB_ASSERT(index>=0 && index<count);
        return ((ElementT*)((char*)this+offset))[index];
    }
};

template<uint16_t ID, typename SchemaT>
struct AggrMsg: public Message<ID, SchemaT> {
    MarketInstrument instrument;
    Group<SubAggr> aggr;
};

template<typename SchemaT=Schema::UDP>
using AggrMsgOnline = AggrMsg<1111, SchemaT>;

template<typename SchemaT=Schema::UDP>
using AggrMsgSnapshot = AggrMsg<1112, SchemaT>;

template<typename SchemaT=Schema::UDP>
struct EmptyBook: public Message<15300, SchemaT> {
    MarketInstrument instrument;
};

} // ns