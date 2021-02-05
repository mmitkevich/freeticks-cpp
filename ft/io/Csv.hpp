#pragma once
#include "ft/core/Fields.hpp"
#include "ft/core/InstrumentsCache.hpp"
#include "toolbox/sys/Time.hpp"
#include <fstream>
#include "ft/io/Sink.hpp"
#include "toolbox/util/Enum.hpp"

namespace ft::io { 


template<class Self, class T, typename FieldT, class OutStreamT>
class BasicStreamSink : public io::BasicSink<Self, T, FieldT> {
    FT_SELF(Self);
    using Base = io::BasicSink<Self, T, FieldT>;
public:
  
};
template<class Self, class ValueT, typename FieldT, class OutStreamT>
class BasicCsvSink : public BasicSink<Self, ValueT, FieldT> {
    using Base = BasicSink<Self, ValueT, FieldT>;
    FT_SELF(Self);
public:
    using typename Base::Type;
    using typename Base::Field;

    using Base::Base;
    using Base::field, Base::flush, Base::size;
    
    void open(std::string_view path) {
        out_.open(path.data(), std::ios_base::out|std::ios_base::trunc);
        self()->write_headers();
        out_.flush();
    }

    void close () {
        out_.close();
    }
    
    void write_headers() {
        for(std::size_t i=0; i<headers_.size(); i++) {
            if(i>0)
                out_ << ", ";
            if(i<headers_.size())
                self()->write_header(i);
        }
        out() << std::endl;   
        flush();     
    }

    void do_flush() {
        out_.flush();
    }

    void write_header(std::size_t i) {
        out_ << headers_[i];
    }

    template<typename T>
    void write_field(const T& val, Field field={}, std::size_t index=-1) {
        if(index!=0)
            out_ << ", ";
        out_ << val;
        if(index==-1) {
            out_ << std::endl;
            self()->flush();
        }
    }

    void operator()(const Type& val) {
        if(!out_.is_open())
            return;
        static_cast<Self*>(this)->write(val);
    }

    void write_field(const Type& val, std::size_t i) {
        val.write_field(out_, field(i));
    }

    auto& out() { return out_; }
protected:
    using Base::headers_, Base::cols_;
    OutStreamT out_;
};

template<class T, typename FieldT = typename T::Field, class OutStreamT=std::ofstream>
class CsvSink : public BasicCsvSink<CsvSink<T,FieldT,OutStreamT>, T, FieldT, OutStreamT> {
    using Base = BasicCsvSink<CsvSink<T,FieldT,OutStreamT>, T, FieldT, OutStreamT>;
  public:
    using Base::Base;
};

template<class T, typename FieldT = typename T::Field, class OutStreamT=std::ofstream>
class SymbolCsvSink : public BasicCsvSink<SymbolCsvSink<T, FieldT, OutStreamT>, T, FieldT, OutStreamT> 
{
    using Base = BasicCsvSink<SymbolCsvSink<T, FieldT, OutStreamT>, T, FieldT, OutStreamT>;
public:
    using typename Base::Field;
    using typename Base::Type;
    
    using Base::field;
    using Base::out;

    SymbolCsvSink(InstrumentsCache& instruments, tb::BitMask<FieldT> &&cols = T::fields())
    : Base(std::move(cols))
    , instruments_(instruments) {}

    void write_header(std::size_t i) {
        Base::write_header(i);
        if(field(i) == Field::InstrumentId) {
            out() << ", Symbol";
        }
    }
    void write_field(const Type& val, std::size_t i) {
        Base::write_field(val, i);        
        if(field(i) == Field::VenueInstrumentId) {
            auto& ins = instruments_[val.venue_instrument_id()];
            out() << ", \"" << ins.symbol() << "\"";
        }
    };
protected:
    InstrumentsCache& instruments_;
};


};