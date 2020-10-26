#pragma once

#include "ft/core/Identifiable.hpp"
#include "ft/core/Stream.hpp"

namespace ft::core {

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
    Unknown = 0,
    Future  = 'f',
    Option  = 'o',
    Repo    = 'r',
    Spot    = 't',    
    Swap    = 's',
    CalendarSpread = 'c',
};
inline std::ostream& operator<<(std::ostream&os, const InstrumentType& self) {
    switch(self) {
        case InstrumentType::Unknown: return os << "Unknown";
        case InstrumentType::Future: return os << "Future";
        case InstrumentType::Option: return os << "Option";
        case InstrumentType::Repo: return os <<"Repo";
        case InstrumentType::Spot: return os << "Spot";
        case InstrumentType::CalendarSpread: return os <<"CalendarSpread";
        default: return os << tbu::unbox(self);
    }
}
class Instrument : public Identifiable {
public:
    // Spot, Future, Option, Swap,...
    InstrumentType type() const { return type_; }
    void type(InstrumentType val) { type_ = val; }
    
    // instrument-specific symbol, e.g. MSFT, AAPL, RIM2020
    std::string_view symbol() const { return symbol_; }
    void symbol(std::string_view val) { symbol_ = val; }
    
    friend std::ostream& operator<<(std::ostream& os, const Instrument& self) {
        os << "type:"<<self.type_<<",symbol:'"<<self.symbol_<<"'";
        return os;
    }
    // JsonDocument extra;
protected:
    InstrumentType type_ {};
    InstrumentStatus status_ {};
    std::string_view symbol_;
};

class VenueInstrument : public Identifiable {
public:
    /// instrument id (non-venue-specific). id() means instrument-at-venue identificator
    Instrument& instrument() { return instrument_; }

    /// native venue-specific instrument identifier, e.g. 
    VenueInstrumentId venue_instrument_id() const { return venue_instrument_id_; }
    void venue_instrument_id(VenueInstrumentId id) { venue_instrument_id_ = id; }

    std::string_view exchange() const { return exchange_; }
    void exchange(std::string_view val) { exchange_ = val; }

    std::string_view venue() const { return venue_; }
    void venue(std::string_view val) { venue_ = val; }

    /// exchange-specific symbol, e.g. MSFT@NASDAQ, AAPL@SPB, RIM2020@MOEX
    std::string symbol() { 
        std::stringstream ss;
        ss << instrument_.symbol();
        ss << "@";
        ss << exchange();
        return ss.str();
    }

    /// venue-specific symbol, e.g. MSFT@SPB.SPB_MD_BIN
    std::string venue_symbol() {
        std::stringstream ss;
        ss << symbol();
        ss << ".";
        ss << venue();
        return ss.str();
    }
    bool empty() const {
        return instrument_.symbol().empty();
    }
    friend std::ostream& operator<<(std::ostream& os, const VenueInstrument& self) {
        os  << "instrument:"<<self.instrument_
            <<",venue_instrument_id:"<<self.venue_instrument_id_
            <<",venue_symbol:"<<self.venue_symbol_
            <<",exchange:"<<self.exchange_
            <<",venue:"<<self.venue_;
        return os;
    }
protected:
    Instrument instrument_;
    VenueInstrumentId venue_instrument_id_{};
    std::string_view venue_symbol_;    
    std::string_view exchange_;
    std::string_view venue_;
};

using VenueInstrumentStream = core::Stream<VenueInstrument>;

};