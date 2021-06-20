#pragma once
#include "ft/core/Component.hpp"
#include "ft/core/Serializer.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/State.hpp"
#include "ft/core/Stream.hpp"
#include "ft/io/Service.hpp"
#include "ft/utils/Mixin.hpp"
#include "toolbox/sys/Time.hpp"
#include <boost/mp11/list.hpp>
#include "toolbox/util/Enum.hpp"
#include <fstream>
#include <type_traits>

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

template<class Self, class ValueT, class SerializerT=FlatSerializer<core::Field>, typename...>
class BasicSink : public core::Component
, public Stream::BasicSlot<Self, const ValueT&>
, public BasicFlushable<Self>
, public BasicParameterized<Self>
, public SinkTag
{
    using Base = core::Component; 
    using Slot = Stream::BasicSlot<Self, const ValueT&>;
    using Flushable = BasicFlushable<Self>;
    FT_SELF(Self);
public:
    using Endpoint = core::StreamTopic;
    using Type = ValueT;
    using Serializer = SerializerT;
    using Field = typename SerializerT::Field;
    using Flushable::flush;

    BasicSink(StreamTopic topic, Component* parent=nullptr, Identifier id={})
    : Base(parent, id)
    {
        self()->topic(topic);
    }

    void open() {}

    void close () {}

    void instruments(core::InstrumentsCache* ins) {
        serializer().instruments(ins);
    }
    /// SyncWriter
    void write(const Type& val) {
        serializer_.write(*self(), val);
        self()->flush();
    }

    /// Slot
    void invoke(const Type& val) {
        self()->write(val);
    }

   /* template<typename T>
    void write_field(Field field, const T& val) {
        assert(false);
    }*/

    void write_eol() {}
    /// AsyncWriter
    template<typename DoneT>
    void async_write(const Type& val, DoneT done) {
        self()->write(val);
        done();
    }

    Serializer& serializer() { return serializer_; }
    const Serializer& serializer() const { return serializer_; }

    tb::BitMask<Field> to_mask(const core::Parameters& params) const {
        tb::BitMask<Field> mask;
        for(auto fld_pa: params) {
            auto fld = serializer().from_string(fld_pa.get_string());
            mask.set(fld);
            TOOLBOX_DEBUG<<"Clickhouse: field '"<<fld<<"' enabled for table '"<<path_<<"'";
        }
        return mask;
    }

    void on_parameters_updated(const core::Parameters& params) {
       self()->configure(params);
    }

    void configure(const core::Parameters& params) {
        path_ = params.str("sink");
        if(params.find("fields")!=params.end())
            serializer().enabled() = serializer().enabled() & to_mask(params["fields"]);
    }
    const std::string& path() const { return path_; }
protected:
    Serializer serializer_;
    std::string path_;
};


/// Multiple sinks
template<class Self,  class MultiServiceT, class ValueL,  template<class...> class SinkM, typename...O>
class BasicMultiSinkService: public io::BasicMultiService<Self, MultiServiceT,  mp::mp_transform<SinkM, ValueL>, O...>
, public BasicSignalSlot<Self>
{
    FT_SELF(Self);
    using Base = io::BasicMultiService<Self, MultiServiceT, mp::mp_transform<SinkM, ValueL>, O...>;
    using SignalSlot = BasicSignalSlot<Self>;
  public:
    using Base::Base;
    using Base::parent;
    using Base::open, Base::close;
    using Base::for_each_service;
    template<class ValueT>
    using Topic = typename SinkM<ValueT>::Endpoint;

    template<class ServiceT>
    void on_new_service(ServiceT& svc) {
    }

    template<class ValueT, typename...ArgsT>
    auto* service(const Topic<ValueT>& endpoint, ArgsT...args) {
        auto* svc =  Base::template service<SinkM<ValueT>, ArgsT...>(endpoint, std::forward<ArgsT>(args)...);
        return svc;
    }

    bool supports(core::StreamTopic topic) {
      switch(topic) {
        case core::StreamTopic::BestPrice:
          return service<core::Tick>(topic)!=nullptr;
        case core::StreamTopic::Instrument:
          return service<core::InstrumentUpdate>(topic)!=nullptr;
      }
      return false;
    }
    core::Stream& slot(core::StreamTopic topic) { 
      switch(topic) {
        case core::StreamTopic::BestPrice: 
          if(auto* svc = service<core::Tick>(topic))
            return *svc; 
        case core::StreamTopic::Instrument:
          if(auto* svc = service<core::InstrumentUpdate>(topic))
            return *svc;
        default:
          throw std::logic_error("no such topic");
      }
      throw std::logic_error("topic not implemented");
    }


    void configure(const core::Parameters& params) {
        Base::configure(params);
        for(auto [id, pa]: params["endpoints"]) {
            auto topic_str = pa.str("topic");
            auto topic = topic_from_name(topic_str);
            if(topic==StreamTopic::Empty) {
                throw std::logic_error("no such topic");
            }
            slot(topic);  // will make_stream;
            for_each_service([&topic, p=pa](auto& svc) {
                //TOOLBOX_DUMP<<"svc.topic "<<svc.topic()<<", topic "<<topic;
                if(svc.topic() == topic) {
                    svc.on_parameters_updated(p);
                }
            });
        }       
    }

    void do_open() {
        Base::do_open();
        MultiServiceT::open();
    }

    void do_close() {
        Base::do_close();
        MultiServiceT::close();
    }
};

template<class ValueL, class BaseServiceT, template<class...> class SinkM>
class MultiSinkService: public BasicMultiSinkService<MultiSinkService<ValueL, BaseServiceT, SinkM>,  BaseServiceT, ValueL, SinkM>
{
    using Base = BasicMultiSinkService<MultiSinkService<ValueL, BaseServiceT, SinkM>, BaseServiceT, ValueL, SinkM>;
public:
    using Base::Base;

    template<typename...ArgsT>
    MultiSinkService(core::InstrumentsCache* ins=nullptr, ArgsT...args)
    : instruments_(ins)
    , Base(std::forward<ArgsT>(args)...) { }

    template<class ServiceT>
    void on_new_service(ServiceT& svc) {
        svc.instruments(instruments_);
    }
    void instruments(core::InstrumentsCache* val) { instruments_ = val; }
    core::InstrumentsCache* instruments() { return instruments_; }
protected:
    core::InstrumentsCache* instruments_{};    
};


} // ft::io