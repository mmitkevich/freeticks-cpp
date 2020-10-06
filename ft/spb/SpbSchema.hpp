#pragma once
#include "ft/utils/Common.hpp"

#include "SpbFrame.hpp"
#include <ostream>

#define SPB_ASSERT assert

namespace ft::spb {
#pragma pack(push, 1)
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

    friend std::ostream& operator<<(std::ostream& os, const Message& self) {
        os << self.frame;
        return os;
    }
};

template<uint16_t ID, typename TraitsT>
struct Snapshot
{
    using Base = Message<ID, TraitsT>;
    Base base;
    Int8 update_seq;
    friend std::ostream& operator<<(std::ostream& os, const Snapshot& self) {
        return os<<self.base;
    }
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
        char *p = (char*)this + offset;
        return (ElementT*)p;
    }
    const ElementT* end() const {
        char *p = (char*)this + offset;
        return (ElementT*)p + count;
    }
    const ElementT& operator[](std::size_t index) const {
        SPB_ASSERT(index>=0 && index<count);
        char *p = (char*)this + offset;
        return ((ElementT*)p)[index];
    }
    friend std::ostream& operator<<(std::ostream&os, const Group& self) {
        os << self.count<<"#[";
        std::size_t i=0;
        for(const auto& e: self) {
            if(i++>0)
                os << ", "; 
            os << e;
        }
        os << "]";
        return os;
    }
};



template<uint16_t ID, typename TraitsT>
struct AggrMsg: public Message<ID, TraitsT>
{
    Instrument instrument_;
    Group<SubAggr> aggr_;

    Instrument& instrument() { return instrument_; }
    Group<SubAggr>& aggr() { return aggr_; }
};

#define FT_BEGIN_ENUM_OSTREAM(Type) 
#define FT_ENUM_OSTREAM(Type, Value) case Type::Value: os << #Value; break;
#define FT_END_ENUM_OSTREAM(Type) } return os; } 
struct SubBest {
    enum class Type: Int1 {
        Buy = 1,
        Sell = 2,
        Deal = 3
    };
    friend auto& operator<<(std::ostream& os, const Type& self) {
        switch(self) {
            case Type::Buy: return os << "Buy";
            case Type::Sell: return os << "Sell";
            case Type::Deal: return os << "Deal";
        }
        return os << (std::int64_t) self;
    }
    enum class Flag: Int1 {
        Update = 0,
        New = 1
    };
    friend auto& operator<<(std::ostream& os, const Flag& self) {
        switch(self) {
            case Flag::Update: return os << "Update";
            case Flag::New: return os << "New";
        }
        return os << (std::int64_t) self;
    }
    Dec8 price;
    Type type;
    Flag flag;
    Int4 amount;
    Time8n time;   

    friend std::ostream& operator<<(std::ostream& os, const SubBest& self) {
        os << "{price:"<<self.price<<",amount:"<<self.amount<<",type:"<<self.type<<",flag:"<<self.flag<<",time:'"<<self.time<<"'}";
        return os;
    } 
};

static_assert(sizeof(SubBest)==22);

template<typename TraitsT>
struct SpbSchema 
{
    // Declare messages
    using SnapshotStart = Snapshot<12345, TraitsT>;
    using SnapshotFinish = Snapshot<12312, TraitsT>;

    using AggrMsgOnline = AggrMsg<1111, TraitsT>;
    using AggrMsgSnapshot = AggrMsg<1112, TraitsT>;

    struct EmptyBook
    {
        using Base = Message<15300, TraitsT>;
        Base base;
        Instrument instrument;
    };
    struct PriceSnapshot
    {
        using Base = Message<7653, TraitsT>;
        Base base;
        Instrument instrument;    
        Group<SubBest> sub_best;
        friend std::ostream& operator<<(std::ostream& os, const PriceSnapshot& self) {
            os << self.base << " ins "<<self.instrument<<" sub_best "<<self.sub_best;
            return os;
        }
    };

    // Declare type list for messages
    using TypeList = mp::mp_list<
        SnapshotStart, SnapshotFinish, PriceSnapshot
        //,AggrMsgOnline, AggrMsgSnapshot, EmptyBook
    >;
};

#pragma pack(pop)


} // ns