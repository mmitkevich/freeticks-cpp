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

enum class InstrumentType: std::uint32_t {
    Unknown = 0,
    Future  = 'f',
    Option  = 'o',
    Repo    = 'r',
    Spot    = 't',    
    Swap    = 's',
    CalendarSpread = 'c',
};

class Instrument : public Identifiable {
public:
    // Spot, Future, Option, Swap,...
    InstrumentType type() { return type_; }
    void type(InstrumentType val) { type_ = val; }
    
    // instrument-specific symbol, e.g. MSFT, AAPL, RIM2020
    std::string_view symbol() { return symbol_; }
    void symbol(std::string_view val) { symbol_ = val; }
    
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
    void instrument(const Instrument& val) { instrument_ = val; }

    /// native venue-specific instrument identifier, e.g. 
    VenueInstrumentId venue_instrument_id() { return venue_instrument_id_; }
    void venue_instrument_id(VenueInstrumentId id) { venue_instrument_id_ = id; }

    std::string_view exchange() { return exchange_; }
    void exchange(std::string_view val) { exchange_ = val; }

    std::string_view venue() { return venue_; }
    void venue(std::string_view val) { venue_ = val; }

    /// exchange-specific symbol, e.g. MSFT@NASDAQ, AAPL@SPB, RIM2020@MOEX
    std::string symbol() { 
        std::stringstream ss;
        ss << instrument().symbol();
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
protected:
    Instrument instrument_;
    VenueInstrumentId venue_instrument_id_{};
    std::string_view venue_symbol_;    
    std::string_view exchange_;
    std::string_view venue_;
};

using VenueInstrumentStream = core::Stream<VenueInstrument>;

};