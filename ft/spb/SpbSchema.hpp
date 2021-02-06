#pragma once
#include "ft/core/SequencedChannel.hpp"
#include "ft/core/Tick.hpp"
#include "ft/utils/Common.hpp"
#include "toolbox/net/Packet.hpp"
#include "SpbFrame.hpp"
#include <cstdint>
#include <ostream>
#include <sstream>
#include <type_traits>

#define SPB_ASSERT assert

namespace ft::spb {

#pragma pack(push, 1)

template<uint16_t ID, typename HeaderT>
struct SpbHeader
{
    static constexpr MsgId msgid {ID};
    Frame frame {ID};
    using Header = HeaderT;
    Header header;
    friend std::ostream& operator<<(std::ostream& os, const SpbHeader& self) {
        return os<<self.frame<<","<<self.header;
    }
    bool empty() const { return frame.msgid==0; }
    std::uint64_t sequence() const { return frame.seq; }
    tb::WallTime server_time() const {return header.system_time.wall_time(); }
};

template<typename HeaderT, typename PayloadT>
struct SpbPacket {
    using Header = HeaderT;
    using Payload = PayloadT;
    
    constexpr static auto msgid = Header::msgid;

    const Header& header() const { return header_; }
    const PayloadT& value() const {return value_; }

    HeaderT header_;
    PayloadT value_;
};

struct Snapshot
{
    Int8 update_seq;
    friend std::ostream& operator<<(std::ostream& os, const Snapshot& self) {
        return os<<"update_seq:"<<self.update_seq;
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

    Group() {}
    Group(const Group& rhs) = delete;
    Group(Group&& rhs) = delete;
    
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
        os << "[";
        std::size_t i=0;
        for(const auto& e: self) {
            if(i++>0)
                os << ","; 
            os << e;
        }
        os << "]";
        return os;
    }
};


struct Aggr
{
    MarketInstrumentId instrument_;
    Group<SubAggr> aggr_;

    MarketInstrumentId& instrument() { return instrument_; }
    Group<SubAggr>& aggr() { return aggr_; }
};

struct Heartbeat 
{
    Int4 reserved_;
};

//UNUSED
//#define FT_BEGIN_ENUM_OSTREAM(Type) 
//#define FT_ENUM_OSTREAM(Type, Value) case Type::Value: os << #Value; break;
//#define FT_END_ENUM_OSTREAM(Type) } return os; } 

struct SubBest {
    enum class Type: Int1 {
        Buy = 1,
        Sell = 2,
        Deal = 3
    };
   
    friend std::ostream& operator<<(std::ostream& os, const Type& self) {
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
    friend std::ostream& operator<<(std::ostream& os, const Flag& self) {
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

struct InstrumentStatus {
    enum class TradingStatus: Int1
    {
        Halt        = 2,
        Trading     = 17,
        NoTrading   = 18,
        Close       = 102,
        ClosePeriod = 103,
        DiscreteAuction = 107,
        Open        = 118,
        FixedPriceAuction = 120
    };
    friend std::ostream& operator<<(std::ostream& os, const TradingStatus& self) {
        switch(self) {
            case TradingStatus::Halt: return os << "Halt";
            case TradingStatus::Trading: return os << "Trading";
            case TradingStatus::NoTrading: return os << "NoTrading";
            case TradingStatus::Close: return os << "Close";
            case TradingStatus::ClosePeriod: return os << "ClosePeriod";
            case TradingStatus::DiscreteAuction: return os << "DiscreteAuction";
            case TradingStatus::Open: return os << "Open";
            case TradingStatus::FixedPriceAuction: return os << "FixedPriceAuction";
        }
        return os << (int) self;
    }
    TradingStatus trading_status;
    Int1 suspend_status;
    Int1 routing_status;
    Int1 reason;

    friend std::ostream& operator<<(std::ostream& os, const InstrumentStatus& self) {
        return os << "trading_status:'" << self.trading_status << "'"
        << ",suspend_status:"<<(int)self.suspend_status
        << ",routing_status:"<<(int)self.routing_status
        << ",reason:"<<(int)self.reason;
    }
};

struct EmptyBook {
    MarketInstrumentId instrument;
};

struct Price
{
    MarketInstrumentId instrument;    
    Group<SubBest> sub_best;
    friend std::ostream& operator<<(std::ostream& os, const Price& self) {
        os << "instrument_id:"<<self.instrument<<",sub_best:"<<self.sub_best;
        return os;
    }
};

struct Instrument {
    InstrumentId instrument_id;
    Chars<32> symbol;
    Chars<64> desc;
    Chars<128> desc_ru;
    InstrumentStatus status;
    Chars<3> type_;
    enum class Type: Int4 {
        Future = 'f',
        TplusN = 't',
        Option = 'o',
        Repo = 'r',
        PR = ('r'<<8)|'p',
        Swap = ('s'<<8)|'w',
        CalendarSpread = 'c',
        DVP = ('d'<<16)|('v'<<8)|'p'
    };
    Type type() const { return *(Type*)type_.c_str(); }
    enum class AuctionDir: Int1 {
        Direct = 0,
        Inverse = 1
    };
    AuctionDir auction_dir;
    Dec8 price_increment;
    Dec8 step_price;
    Int2 legs_count;
    Int2 trade_mode_id;

    friend std::ostream& operator<<(std::ostream& os, const Instrument& self) {
        //std::wcout << self.symbol.wstr() << std::endl;//<<"|"<<self.desc_ru.str() << std::endl;

        return os  << "instrument_id:"<<self.instrument_id
            << ",symbol:'" << self.symbol.str() <<"'"
            << ",desc:'" << self.desc.str() <<"'"
            << ",desc_ru:'" << self.desc.str() <<"'"
            << ",status:{" << self.status << "}"
            << ",type:'" << self.type() <<"=" << self.type_ <<"'"
            << ",auction_dir:'" << self.auction_dir<<"'"
            << ",price_increment:" << self.price_increment
            << ",step_price:" << self.step_price
            << ",legs_count:" << self.legs_count
            << ",trade_mode_id:" << self.trade_mode_id;
    }
    friend std::ostream& operator<<(std::ostream& os, const AuctionDir& self) {
        switch(self) {
            case AuctionDir::Direct: return os<<"Direct";
            case AuctionDir::Inverse: return os<<"Inverse";
            default: return os << tb::unbox(self);
        }
    }
    friend std::ostream& operator<<(std::ostream& os, const Type& self) {
        switch(self) {
            case Type::Future: return os<< "Future";
            case Type::TplusN: return os<<"T+N";
            case Type::Option: return os<<"Option";
            case Type::Repo: return os<<"Repo";
            case Type::PR: return os<<"pr";
            case Type::Swap: return os<<"Swap";
            case Type::CalendarSpread: return os<<"CalendarSpread";
            case Type::DVP: return os<<"dvp";
            default: return os << tb::unbox(self);
        }
    }
};

template<class HeaderT, class EndpointT>
struct SpbSchema
{
    static constexpr std::string_view Exchange = "XPET";
    static constexpr std::string_view Venue = "SPB_MDBIN";

    // Declare messages
    using SnapshotStart = SpbPacket<SpbHeader<12345, HeaderT>, Snapshot>;
    using SnapshotFinish = SpbPacket<SpbHeader<12312, HeaderT>, Snapshot>;

    using AggrOnline = SpbPacket<SpbHeader<1111, HeaderT>, Aggr>;
    using AggrSnapshot = SpbPacket<SpbHeader<1112, HeaderT>, Aggr>;
  
    using PriceOnline =  SpbPacket<SpbHeader<7651, HeaderT>, Price>;
    using PriceSnapshot = SpbPacket<SpbHeader<7653, HeaderT>, Price>;

    using InstrumentSnapshot = SpbPacket<SpbHeader<973, HeaderT>, Instrument>;

    using Heartbeat = SpbPacket<SpbHeader<15236, HeaderT>, spb::Heartbeat>;

    using Endpoint = EndpointT;
    using BinaryPacket = tb::Packet<tb::ConstBuffer, Endpoint>;
    template<class HandlerT> 
    using Channel = BasicSequencedChannel<HandlerT, Endpoint, spb::Seq>;

    template<class MessageT, class BinaryPacketT=BinaryPacket> 
    class Packet: public tb::PacketView<MessageT, BinaryPacketT> {
        using Base = tb::PacketView<MessageT, BinaryPacketT>;
    public:
        using Base::Base;
        using Base::header;
        using Base::value;
        std::uint64_t sequence() const { return value().header().frame.seq; }
    };

    static constexpr std::int64_t PriceMultiplier = 100'000'000;
    using PriceConv = core::PriceConversion<std::int64_t, PriceMultiplier>;
    using QtyConv = core::QtyConversion<std::int64_t, 1>;
};


template<typename T>
struct SeqValue {
    std::uint64_t seq;
    T value;
};

struct SnapshotKey {
    spb::MarketInstrumentId instrument_id;
    spb::SourceId source_id;
    std::uint64_t to_long() const {
        return ((((std::uint64_t)instrument_id.instrument_id)<<16 | ((std::uint64_t)instrument_id.market_id)) << 16) | source_id;
    }
    bool operator==(const SnapshotKey& rhs) const { return to_long() == rhs.to_long(); }
};

#pragma pack(pop)

} // ns


namespace std {

template<>
struct hash<ft::spb::SnapshotKey> {
    std::size_t operator()(const ft::spb::SnapshotKey& val) const {
        return hash<std::uint64_t>{}(val.to_long());
    }
};


}