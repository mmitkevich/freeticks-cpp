#pragma once

#include "ft/core/Stream.hpp"
#include "ft/core/Object.hpp"

namespace ft { inline namespace core {

class InstrumentStatus
{
public:
    enum class TradingPhase: std::uint16_t {
        Unknown         = 0, 
        PreOpen         = 1,
        Opening         = 2, 
        OpeningAuction  = 3,
        Trading         = 4,
        IntradayAuction = 5, 
        PreClose        = 6,
        Closing         = 7,
        ClosingAuction  = 8,
        LastAuction     = 9,
        PostTrading     = 10,
        Closed          = 11, 
        Halted          = 12,
        Suspended       = 13, 
        Fast            = 14,
        Buyback         = 15,
        ContinuousAuction = 16
    };
    bool trading() const { 
        switch(phase()) {
            case TradingPhase::Trading:
            case TradingPhase::PreClose: 
                return true;
            default:
                return false;
        }
    }
    TradingPhase phase() const { return phase_;}
    void phase(TradingPhase val) { phase_ = val; }
    // JsonDocument extra;
private:
    TradingPhase phase_ {};
};

enum class InstrumentType: std::int8_t {
    Empty = 0,
    Future  = 'f',
    Option  = 'o',
    Repo    = 'r',
    Stock   = 't',    
    Swap    = 's',
    CalendarSpread = 'c',
};

inline std::ostream& operator<<(std::ostream&os, const InstrumentType& self) {
    switch(self) {
        case InstrumentType::Empty: return os << "Empty";
        case InstrumentType::Future: return os << "Future";
        case InstrumentType::Option: return os << "Option";
        case InstrumentType::Repo: return os <<"Repo";
        case InstrumentType::Stock: return os << "Stock";
        case InstrumentType::CalendarSpread: return os <<"Calendar";
        default: return os << tb::unbox(self);
    }
}

// Pad
template<std::size_t DataLength=0>
class BasicInstrumentUpdate : public ft_instrument_t {
    using Base = ft_instrument_t;
public:
    template<typename InstrumentT>
    explicit BasicInstrumentUpdate(const InstrumentT& ins) {
        std::memset(this, 0, sizeof(ft_instrument_t));
        symbol(ins.symbol());
        ft_venue_instrument_id = ins.venue_instrument_id();
    }
    BasicInstrumentUpdate() {
        std::memset(this, 0, sizeof(ft_instrument_t));
    }
    StreamTopic topic() const { return (StreamTopic) ft_hdr.ft_type.ft_topic; }
    void topic(StreamTopic val) { ft_hdr.ft_type.ft_topic = tb::unbox(val); }

    VenueInstrumentId venue_instrument_id() const { return ft_venue_instrument_id; }
    void venue_instrument_id(VenueInstrumentId val) { ft_venue_instrument_id = val; }

    InstrumentId instrument_id() const { return ft_instrument_id;}
    void instrument_id(InstrumentId val) { ft_instrument_id = val; }

    std::size_t bytesize() const noexcept { return ft_hdr.ft_len + ft_symbol_len + ft_exchange_len + ft_venue_symbol_len; }

    std::string_view symbol() const { return std::string_view(data_, ft_symbol_len); }
    std::string_view exchange() const { return std::string_view(data_+ft_symbol_len, ft_exchange_len); }
    std::string_view venue_symbol() const { return std::string_view(data_+ft_symbol_len+ft_exchange_len, ft_venue_symbol_len); }    
    std::string exchange_symbol() const {
        std::stringstream ss;
        ss<<symbol();
        if(!exchange().empty()) {
            ss<<"|"<<exchange();
        }
        return ss.str();
    }
    InstrumentType instrument_type() const { return static_cast<InstrumentType>(ft_instrument_type); }
    void instrument_type(InstrumentType val) { ft_instrument_type = tb::unbox(val); }

    void symbol(std::string_view val) {
        assert(ft_venue_symbol_len==0); // should be called before venue_symbol and before exchange
        assert(ft_exchange_len==0);
        ft_symbol_len = val.size();
        std::memcpy(const_cast<char*>(symbol().data()), val.data(), val.size());
        update_hdr();
    }
    void exchange(std::string_view val) {
        assert(ft_venue_symbol_len==0); // should be called before venue_symbol
        ft_exchange_len = val.size();
        std::memcpy(const_cast<char*>(exchange().data()), val.data(), val.size());
        update_hdr();
    }
    void venue_symbol(std::string_view val) {
        ft_venue_symbol_len = val.size();
        std::memcpy(const_cast<char*>(venue_symbol().data()), val.data(), val.size());
        update_hdr();
    }
    void update_hdr() {
        ft_hdr.ft_len = sizeof(Base)+ft_symbol_len+ft_exchange_len+ft_venue_symbol_len;
    }
    friend std::ostream& operator<<(std::ostream& os, const BasicInstrumentUpdate<DataLength>& self) {
        os << "sym:'"<<self.symbol()<<"', t:'"<<self.topic()<<"', mic:'"<<self.exchange()<<"'";
        // os <<"', it:'"<<self.instrument_type()<<"'";
        os << ", viid:"<<self.venue_instrument_id();
        os << ", vsym:'"<<self.venue_symbol()<<"'";
        return os;
    }
    template<std::size_t NewSizeI>
    BasicInstrumentUpdate<NewSizeI>& as_size() {
        return *reinterpret_cast<BasicInstrumentUpdate<NewSizeI>*>(this);
    }
    template<std::size_t NewSizeI>
    const BasicInstrumentUpdate<NewSizeI>& as_size() const {
        return *reinterpret_cast<const BasicInstrumentUpdate<NewSizeI>*>(this);
    }
private:
    ft_char_t data_[DataLength];
};

using InstrumentUpdate = BasicInstrumentUpdate<0>;

using InstrumentStream = core::Stream<const core::InstrumentUpdate &>;
using InstrumentSink = core::Sink<const core::InstrumentUpdate &>;


class Instrument : public BasicObject<Instrument> {
    using Base = BasicObject<Instrument>;
public:
    using Base::Base;

    // Spot, Future, Option, Swap,...
    InstrumentType instrument_type() const { return instrument_type_; }
    void instrument_type(InstrumentType val) { instrument_type_ = val; }
    
    // well-known ticker e.g. MSFT, AAPL, RIM2020
    std::string_view symbol() const { return symbol_; }
    void symbol(std::string_view val) { symbol_ = val; }
    
    bool empty() const noexcept { return symbol_.empty(); } // FIXME

    friend std::ostream& operator<<(std::ostream& os, const Instrument& self) {
        os << "sym:'"<<self.symbol()<<"', it:'"<<self.instrument_type()<<"', id:"<<self.id();
        return os;
    }
    // JsonDocument extra;
protected:
    InstrumentType instrument_type_ {};
    std::string_view symbol_;
};

// Instrument + associated Exchange
class ListedInstrument : public BasicObject<ListedInstrument> {
    using Base = BasicObject<ListedInstrument>;
public:
    using Base::Base;

    /// instrument id (non-venue-specific). id() means instrument-at-venue identificator
    Instrument& instrument() { return instrument_; }
    const Instrument& instrument() const { return instrument_; }

   // Spot, Future, Option, Swap,...
    InstrumentType instrument_type() const { return instrument_.instrument_type(); }
    void instrument_type(InstrumentType val) { instrument_.instrument_type(val); }
    
    // well-known ticker e.g. MSFT, AAPL, RIM2020
    std::string_view symbol() const { return instrument_.symbol(); }
    void symbol(std::string_view val) { instrument_.symbol(val); }
    
    bool empty() const noexcept { return instrument_.empty(); } // FIXME

    /// exchange name (MIC)
    std::string_view exchange() const { return exchange_; }
    void exchange(std::string_view val) { exchange_ = val; }

    /// exchange-specific symbol, e.g. MSFT@NASDAQ, AAPL@SPB, RIM2020@MOEX
    std::string exchange_symbol() const { 
        std::stringstream ss;
        ss << symbol();
        ss << '@';
        ss << exchange();
        return ss.str();
    }

    friend std::ostream& operator<<(std::ostream& os, const ListedInstrument& self) {
        os << "sym:'"<<self.symbol()<<"', it:'"<<self.instrument_type()<<"', id:"<<self.id();
        return os;
    }
protected:
    Instrument instrument_;
    std::string_view exchange_;
};

class VenueInstrument : public BasicObject<VenueInstrument> {
    using Base = BasicObject<VenueInstrument>;
public:
    using Base::Base;
    
    /// instrument id (non-venue-specific). id() means instrument-at-venue identificator
    Instrument& instrument() { return listed_instrument_.instrument(); }
    const Instrument& instrument() const { return listed_instrument_.instrument(); }

    InstrumentId instrument_id() const { return instrument().id(); }
    void instrument_id(InstrumentId val) { instrument().id(val); }

    InstrumentType instrument_type() const { return instrument().instrument_type(); }
    void instrument_type(InstrumentType val) { instrument().instrument_type(val); }

    /// unlike internal id of this object, venue could also have specific numeric id
    VenueInstrumentId venue_instrument_id() const { return venue_instrument_id_; }
    void venue_instrument_id(VenueInstrumentId id) { venue_instrument_id_ = id; }

    /// exchange name (MIC)
    std::string_view exchange() const { return listed_instrument_.exchange(); }
    void exchange(std::string_view val) { listed_instrument_.exchange(val); }

    /// venue name
    std::string_view venue() const { return venue_; }
    void venue(std::string_view val) { venue_ = val; }

    // standalone instrument symbol, e.g. MSFT
    std::string_view symbol() const { return listed_instrument_.symbol(); }
    void symbol(std::string_view val) { listed_instrument_.symbol(val); }

    /// exchange-specific symbol, e.g. MSFT|NASDAQ, AAPL|SPB, RIM2020|MOEX
    std::string exchange_symbol() const { 
        std::stringstream ss;
        ss << symbol();
        if(!exchange().empty()) {
            ss << "|";
            ss << exchange();
        }
        return ss.str();
    }

    /// venue-specific symbol, e.g. MSFT@SPB.SPB_MD_BIN
    std::string venue_symbol() const {
        std::stringstream ss;
        ss << symbol();
        if(!exchange().empty()) {
            ss << "|";
            ss << exchange();
        }
        if(!venue().empty()) {
            ss << "|";
            ss << venue();
        }
        return ss.str();
    }

    bool empty() const {
        return listed_instrument_.empty();
    }

    friend std::ostream& operator<<(std::ostream& os, const VenueInstrument& self) {
        os  << "sym:'"<<self.venue_symbol()<<"'"
            << ", iid:" << self.id();
        if(!self.venue_instrument_id().empty())
            os << ",viid:"<<self.venue_instrument_id_;
        if(!self.venue_symbol().empty())
            os << ",vsym:'"<<self.venue_symbol_<<"'";
        return os;
    }
protected:
    ListedInstrument listed_instrument_;
    VenueInstrumentId venue_instrument_id_ {};
    std::string_view venue_symbol_;    
    std::string_view venue_;
};

}} // ft::core