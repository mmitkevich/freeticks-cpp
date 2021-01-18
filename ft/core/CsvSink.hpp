#pragma once
#include "ft/core/Fields.hpp"
#include "ft/core/InstrumentsCache.hpp"
#include "toolbox/sys/Time.hpp"
#include <fstream>

namespace ft { inline namespace core {

template<class Self, class T, typename FieldsT, class OutStreamT>
class BasicCsvSink {
    FT_MIXIN(Self)
public:
    using Type = T;
    using Fields = FieldsT;

    BasicCsvSink(std::vector<Fields> &&cols = T::fields(), std::vector<std::string> &&headers={})
    : cols_(std::move(cols))
    , headers_(std::move(headers))
    {
        while(headers_.size()<cols_.size()) {
            std::stringstream ss;
            ss << cols_[headers_.size()];
            headers_.push_back(ss.str());
        }
    }

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

    void flush(bool force=false) {
        tb::MonoTime now;
        using namespace std::chrono_literals;
        if(force || (now=tb::MonoClock::now())-last_flush_>=1s) {
            last_flush_ = now;
            out().flush();
        }
    }

    void write_header(std::size_t i) {
        out_ << headers_[i];
    }

    void write_row(const Type& val) {
        for(std::size_t i=0; i<cols_.size(); i++) {
            if(i>0)
                out_ << ", ";
            self()->write_field(val, i);
        }
        out() << std::endl;
        flush();
        // TODO: time-based auto flush policy   
        // out_.flush();   
    }

    void operator()(const Type& val) {
        if(!out_.is_open())
            return;
        static_cast<Self*>(this)->write_row(val);
    }

    void write_field(const Type& val, std::size_t i) {
        val.write_field(out_, field(i));
    }
    
    Fields field(std::size_t i) { return cols_[i]; }

    auto& out() { return out_; }
protected:
    tb::MonoTime last_flush_ = tb::MonoClock::now();
    std::vector<Fields> cols_;
    std::vector<std::string> headers_;
    OutStreamT out_;
};

template<class T, typename FieldsT = typename T::Fields, class OutStreamT=std::ofstream>
class CsvSink : public BasicCsvSink<CsvSink<T,FieldsT,OutStreamT>, T, FieldsT, OutStreamT> {
    using Base = BasicCsvSink<CsvSink<T,FieldsT,OutStreamT>, T, FieldsT, OutStreamT>;
  public:
    using Base::Base;
};

template<class T, typename FieldsT = typename T::Fields, class OutStreamT=std::ofstream>
class SymbolCsvSink : public BasicCsvSink<SymbolCsvSink<T, FieldsT, OutStreamT>, T, FieldsT, OutStreamT> 
{
    using Base = BasicCsvSink<SymbolCsvSink<T, FieldsT, OutStreamT>, T, FieldsT, OutStreamT>;
public:
    using typename Base::Fields;
    using typename Base::Type;
    
    using Base::field;
    using Base::out;

    SymbolCsvSink(InstrumentsCache& instruments, std::vector<Fields> &&cols = T::fields(), std::vector<std::string> &&headers={})
    : Base(std::move(cols), std::move(headers))
    , instruments_(instruments) {}

    void write_header(std::size_t i) {
        Base::write_header(i);
        if(field(i) == Fields::InstrumentId) {
            out() << ", Symbol";
        }
    }
    void write_field(const Type& val, std::size_t i) {
        Base::write_field(val, i);        
        if(field(i) == Fields::VenueInstrumentId) {
            auto& ins = instruments_[val.venue_instrument_id()];
            out() << ", \"" << ins.symbol() << "\"";
        }
    };
protected:
    InstrumentsCache& instruments_;
};


}};