#pragma once
#include "ft/utils/Common.hpp"

#include "SpbFrame.hpp"

#define SPB_ASSERT assert

namespace ft::spb {

struct SpbUdp
{
    using Header = MdHeader;
};

template<uint16_t ID, typename TraitsT>
struct Message
{
    static constexpr MsgId msgid {ID};
    Frame frame {ID};
    using Header = typename TraitsT::Header;
    Header header;
};

template<uint16_t ID, typename TraitsT>
struct Snapshot: public Message<ID, TraitsT>
{
    Int8 update_seq;
};

struct Side
{
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

struct NewFlag
{
    enum Value:char {
        UPDATE = 0,
        NEW = 1
    };
    Value value = UPDATE;
};

using Amount = Int4;

struct SubAggr
{
    Dec8 price;
    Side side;
    NewFlag flag;
    Amount amount;
    Time8n time;
};

template<typename ElementT>
struct Group
{
    Int2 offset;
    Int2 count;

    using const_iterator = const ElementT*;

    std::size_t size() const {
        return count;
    }
    const ElementT* begin() const {
        return ((ElementT*)((char*)this)+offset);
    }
    const ElementT* end() const {
        return ((ElementT*)((char*)this)+offset)+count;
    }
    ElementT& operator[](std::size_t index) {
        SPB_ASSERT(index>=0 && index<count);
        return ((ElementT*)((char*)this)+offset)[index];
    }
};



template<uint16_t ID, typename TraitsT>
struct AggrMsg: public Message<ID, TraitsT>
{
    Instrument instrument;
    Group<SubAggr> aggr;
};

struct SubBest {
    Dec8 price;
    Int1 type;
    Int1 flag;
    Int4 amount;
    Time8n time;    
};

template<typename TraitsT>
struct SpbSchema 
{
    // Declare messages
    using SnapshotStart = Snapshot<12345, TraitsT>;
    using SnapshotFinish = Snapshot<12312, TraitsT>;

    using AggrMsgOnline = AggrMsg<1111, TraitsT>;
    using AggrMsgSnapshot = AggrMsg<1112, TraitsT>;

    struct EmptyBook: Message<15300, TraitsT>
    {
        Instrument instrument;
    };
    struct PriceSnapshot : Message<7653, TraitsT>
    {
        Instrument instrument;    
        Group<SubBest> sub_best;
    };

    // Declare type list for messages
    using TypeList = mp::mp_list<
        SnapshotStart, SnapshotFinish, PriceSnapshot
        //,AggrMsgOnline, AggrMsgSnapshot, EmptyBook
    >;
};



} // ns