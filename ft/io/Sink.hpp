#pragma once
#include "ft/core/Component.hpp"
#include "ft/core/CoreFields.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/State.hpp"
#include "ft/io/Service.hpp"
#include "ft/utils/Mixin.hpp"
#include "toolbox/sys/Time.hpp"
#include <boost/mp11/list.hpp>

#include <fstream>

namespace ft::io { 


template<class Self>
class BasicFlushable {
    FT_SELF(Self);
public:
    void flush(bool force = false) {
        tb::MonoTime now;
        using namespace std::chrono_literals;
        tb::Duration elapsed;
        if(force || (elapsed=(now=tb::MonoClock::now())-last_flush_)>=flush_interval_) {
            last_flush_ = now;
            self()->do_flush();
        }
    }
    void do_flush() {}  
protected:
    tb::Duration flush_interval_ = std::chrono::seconds(1);
    tb::MonoTime last_flush_ = tb::MonoClock::now();
};

template<class Self, class ValueT, class FieldsT, typename...>
class BasicSink : public core::Component
, public Stream::BasicSlot<Self, const ValueT&>
, public BasicFlushable<Self>
{
    using Base = core::Component; 
    using Slot = Stream::BasicSlot<Self, const ValueT&>;
    using Flushable = BasicFlushable<Self>;
    FT_SELF(Self);
public:
    using Type = ValueT;
    using Fields = FieldsT;
    using Field = typename Fields::Field;
    using Flushable::flush;
    
    BasicSink(Fields &&fields = ValueT::template fields<typename Fields::Field>(), Component* parent=nullptr, Identifier id={})
    : Base(parent,id)
    , fields_(std::move(fields))
    {
    }

    void open() {}

    void close () {}
    
    void write_header() {}

    /// SyncWriter
    void write(const Type& val) {
        std::size_t index = 0;
        fields_.for_each(val, [&](Field field, const auto& val) { 
            if(field!=Field::Empty) {
                self()->write_field(val, field , index++);
            } else {
                index = 0;
            }
        });
        self()->flush();
    }

    /// Slot
    void invoke(const Type& val) {
        self()->write(val);
    }

    template<typename T>
    void write_field(const T& val, Field field={}, std::size_t index=-1) {
        assert(false);
    }
    /// AsyncWriter
    template<typename DoneT>
    void async_write(const Type& val, DoneT done) {
        self()->write(val);
        done();
    }

    Fields& fields() { return fields_; }
    const Fields& fields() const { return fields_; }
protected:
    Fields fields_;
};


} // ft::io