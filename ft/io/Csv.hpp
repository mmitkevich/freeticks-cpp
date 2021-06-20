#pragma once
#include "ft/core/Fields.hpp"
#include "ft/core/InstrumentsCache.hpp"
#include "ft/core/Serializer.hpp"
#include "toolbox/sys/Time.hpp"
#include <fstream>
#include <type_traits>
#include "ft/io/Sink.hpp"
#include "toolbox/util/Enum.hpp"

namespace ft::io { 


template<class Self, class ValueT, class SerializerT, class OutStreamT=std::ofstream, typename...>
class BasicCsvSink : public BasicSink<Self, ValueT, SerializerT> {
    using Base = BasicSink<Self, ValueT, SerializerT>;
    FT_SELF(Self);
public:
    using typename Base::Type;
    using typename Base::Field;

    using Base::Base;
    using Base::flush, Base::path, Base::serializer;

    void open() {
        out_.open(path().data(), std::ios_base::out|std::ios_base::trunc);
        colno_ = 0;
        self()->write_header();
        out_.flush();
    }

    void close () {
        out_.close();
        colno_ = 0;
    }
    
    void do_flush() {
        out_.flush();
    }

    void write_header(Field field) {
        if(colno_!=0)
            out_ << ", ";        
        out_ << field;
        colno_++;
    }

    void write_eol() {
        out_ << std::endl;
        colno_ = 0;
    }

    void write_header() {
        serializer().write_header(*this, tb::type_c<ValueT>{});
        self()->write_eol();
    }

    template<class T>
    void write_field(Field field, const T& val) {
        if(colno_!=0)
            out_ << ", ";
        self()->do_write(val);
        colno_++;
    }
    template<class T>
    void do_write(const T& val) { out_ << val; }
    void do_write(const std::nullptr_t& val) {}
    void do_write(const ft::Timestamp& ts) { out_ << toolbox::sys::put_time<toolbox::Nanos>(ts); }

    void operator()(const Type& val) {
        if(!out_.is_open())
            return;
        static_cast<Self*>(this)->write(val);
    }

    auto& out() { return out_; }
protected:
    OutStreamT out_;
    std::size_t colno_ = 0;
};

template<
class ValueT /// POD type
, class SerializerT = FlatSerializer<core::Field>
, class OutStreamT=std::ofstream>
class CsvSink : public BasicCsvSink<CsvSink<ValueT,SerializerT,OutStreamT>, ValueT, SerializerT, OutStreamT> {
    using Base = BasicCsvSink<CsvSink<ValueT,SerializerT,OutStreamT>, ValueT, SerializerT, OutStreamT>;
  public:
    using Base::Base;
};



};