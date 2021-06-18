#pragma once
#include "clickhouse/columns/column.h"
#include "clickhouse/columns/date.h"
#include "clickhouse/columns/enum.h"
#include "clickhouse/columns/numeric.h"
#include "clickhouse/columns/string.h"
#include "clickhouse/columns/uuid.h"
#include "clickhouse/types/types.h"
#include "ft/core/Component.hpp"
#include "ft/core/Fields.hpp"
#include "ft/core/InstrumentsCache.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/State.hpp"
#include "ft/core/Stream.hpp"
#include "ft/core/Tick.hpp"
#include "ft/io/Protocol.hpp"
#include "toolbox/net/Endpoint.hpp"
#include "toolbox/net/ParsedUrl.hpp"
#include "toolbox/sys/Log.hpp"
#include "toolbox/sys/Time.hpp"
#include "clickhouse/client.h"
#include "ft/io/Service.hpp"
#include "ft/io/Sink.hpp"
#include "toolbox/util/Enum.hpp"
#include <cstdint>
#include <exception>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>

namespace ft::io { 

using namespace clickhouse;

template<class T, bool flag=false>  
struct static_not_supported {
    constexpr static_not_supported() { static_assert(flag, "type not supported"); }
};

class ClickHouseConn : public io::Service  {
    using Base = io::Service;
public:   
    using Base::Base;

    void url(std::string_view url) {
        url_ = tb::ParsedUrl(url);
    }
    void open() {
        try {
            client_ =  std::make_unique<Client>(to_options(url_));
            TOOLBOX_INFO << "Clickhouse connected: "<<url_.url();
        }catch(std::exception& e) {
            TOOLBOX_ERROR<<"Failed to connect to clickhouse: "<< url_.url()<<"; error: "<<e.what();
            throw;
        }
    }
    void close() {
        client_.reset();
    }
    static ClientOptions to_options (const toolbox::ParsedUrl& url) {
        auto opts =  clickhouse::ClientOptions();
        if(url.host().size()>0)
            opts.SetHost(std::string{url.host()});
        if(url.service().size()>0)
            opts.SetPort(std::atoi(url.service().data()));
        return opts;
    }
    Client& socket() { return *client_; }
    const tb::ParsedUrl& url() const { return url_; }
protected:
    std::unique_ptr<Client> client_;
    tb::ParsedUrl url_;
};

template<typename T>
struct ClickHouseEnum{};

template<>
struct ClickHouseEnum<core::Event> {
    static inline const clickhouse::TypeRef enumtype = clickhouse::Type::CreateEnum8({
        {"SNAPSHOT",1}, {"UPDATE",2}});
};

template<>
struct ClickHouseEnum<core::TickEvent> {
    static inline const clickhouse::TypeRef enumtype = clickhouse::Type::CreateEnum8({
        {"ADD",1}, {"DELETE",2}, {"MODIFY",3}, {"CLEAR",4}, {"TRADE", 5}});
};

template<>
struct ClickHouseEnum<core::TickSide> {
    static inline const clickhouse::TypeRef enumtype = clickhouse::Type::CreateEnum8({
        {"EMPTY",0 }, {"BUY",1}, {"SELL",-1}});
};
/// used to store
template<class Self, class ValueT, class FieldsT, typename...O>
class BasicClickHouseSink : public io::BasicSink<Self, ValueT, FieldsT, O...>
{
    FT_SELF(Self);
    using Base = io::BasicSink<Self, ValueT, FieldsT, O...>;
public:
    using typename Base::Type;
    using Base::parent, Base::fields;
    using Endpoint = core::StreamTopic;

    template<typename...ArgsT>
    BasicClickHouseSink(StreamTopic topic, ArgsT...args)
    : Base(std::forward<ArgsT>(args)...) {
        self()->topic(topic);
    }
    
    template<class ColumnT, typename...A>
    ColumnT& get_column(Field field, A...args) {
        auto it = cols_.find(field);
        if(it!=cols_.end())
            return static_cast<ColumnT&>(*it->second);
        cols_[field] = ColumnRef(new ColumnT(std::forward<A>(args)...));
        auto name_it = col_names_.find(field);
        if(name_it==col_names_.end()) { {
            auto name = to_string(field);
            assert(!name.empty());
            col_names_[field] = name;
            TOOLBOX_DEBUG<<"Clickhouse: automatic field '"<<field<<"' to column '"<<name<<"' mapping added";
        }
        }
        return static_cast<ColumnT&>(*cols_[field]);
    }

    void instruments(core::InstrumentsCache* ins) {
        fields().instruments(ins);
    }

    /// get or add column for type T
    template<typename T>
    void append_column(Field field, const T& val) {
        if constexpr (std::is_same_v<T, Timestamp>) {
            //TOOLBOX_DUMP<<"append_column<fb::Timestamp> field:"<<field<<", val: "<<toolbox::sys::put_time<toolbox::Nanos>(val);                    
            get_column<ColumnDateTime64>(field, 9).Append(val.time_since_epoch().count());
        } else if constexpr(std::is_enum_v<T>) {
            using U = std::underlying_type_t<T>;
            if constexpr(std::is_same_v<U, char> || std::is_same_v<U, unsigned char>) {
                //TOOLBOX_DUMP<<"append_column<enum8> field:"<<field<<", val: "<<val;
                get_column<ColumnEnum8>(field, ClickHouseEnum<T>::enumtype).Append((unsigned char)tb::unbox(val));
                //get_column<ColumnUInt8>(field).Append((unsigned char)tb::unbox(val));
            } else  if constexpr(std::is_same_v<U, short> || std::is_same_v<U, unsigned short>) {
                //TOOLBOX_DUMP<<"append_column<enum16> field:"<<field<<", val: "<<val;
                get_column<ColumnEnum16>(field, ClickHouseEnum<T>::enumtype).Append((unsigned short) tb::unbox(val));
                //get_column<ColumnUInt16>(field).Append((unsigned char)tb::unbox(val));
            } else {
                TOOLBOX_WARNING<<"append_column<enum16> field:"<<field<<", val: "<<val<<" was truncated to 16 bits";
                get_column<ColumnEnum16>(field, ClickHouseEnum<T>::enumtype).Append((unsigned short) tb::unbox(val));
            }
        } else if constexpr (std::is_integral_v<T> && !std::is_signed_v<T>) {
            //TOOLBOX_DUMP<<"append_column<unsigned integral> field:"<<field<<", val: "<<val;
            get_column<ColumnUInt64>(field).Append(val);
        } else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>) {
            //TOOLBOX_DUMP<<"append_column<signed integral> field"<<field<<", val: "<<val;
            get_column<ColumnInt64>(field).Append(val);
        } else if constexpr(std::is_floating_point_v<T>) {
            //TOOLBOX_DUMP<<"append_column<floating_point> field:"<<field<<", val: "<<val;            
            get_column<ColumnFloat64>(field).Append(val);
        } else if constexpr(std::is_same_v<T, Identifier>) {
            //TOOLBOX_DUMP<<"append_column<ft::Identifier> field:"<<field<<", val: "<<val;
            get_column<ColumnUUID>(field).Append(val.pair());
        } else if constexpr(std::is_same_v<T, std::string_view>) {
            //TOOLBOX_DUMP<<"append_column<std::string_view> field"<<field<<", val: "<<val;
            get_column<ColumnString>(field).Append(val);
        } else if constexpr(std::is_same_v<T, std::string>) {
            //TOOLBOX_DUMP<<"append_column<std::string> field:"<<field<<", val: "<<val;
            get_column<ColumnString>(field).Append(val);
        } else if constexpr(std::is_same_v<T, const char*>) {
            //TOOLBOX_DUMP<<"append_column<const char*> field:"<<field<<", val: "<<val;
            get_column<ColumnString>(field).Append(val);
        }  else if constexpr(std::is_same_v<T, std::nullptr_t> ) {
            // end of message
        } else {
            static_not_supported<T>();
        }
    }

    void close () {
        self()->flush(true);
    }

    void do_flush() {
        auto* svc = static_cast<ClickHouseConn*>(self()->parent());
        auto& socket = svc->socket();
        Block block;
        std::size_t ncols = 0;
        std::size_t nrows = 0;
        for(auto& it: cols_) {
            std::string& name = col_names_[it.first];
            assert(!name.empty());
            block.AppendColumn(name, it.second);
            nrows = std::max(nrows, it.second->Size());
            ncols++;
        }
        if(ncols>0 && nrows>0) {
            TOOLBOX_INFO<<"Clickhouse: flushing nrows:"<<nrows<<", ncols:"<<ncols<<" into table:"<<table_;
            socket.Insert(table_, block);
        }
        cols_.clear();        
    }
    
    static Int64 to_datetime64(core::Timestamp val) {
        return val.time_since_epoch().count();
    }

    template<typename T>
    void write_field(const T& val, Field field, int index=-1) {
        append_column(field, val);
    }
    
    tb::BitMask<Field> to_mask(const core::Parameters& params) const {
        tb::BitMask<Field> mask;
        for(auto fld_pa: params) {
            auto fld = fields().from_string(fld_pa.get_string());
            mask.set(fld);
            TOOLBOX_DEBUG<<"Clickhouse: field '"<<fld<<"' enabled for table '"<<table_<<"'";
        }
        return mask;
    }
    void on_parameters_updated(const core::Parameters& params) {
        table_ = params.str("table");
        if(params.find("fields")!=params.end())
            fields().enabled() = fields().enabled() & to_mask(params["fields"]);
    }
protected:
    std::string table_;
    tb::unordered_map<Field, ColumnRef> cols_;
    tb::unordered_map<Field, std::string> col_names_;
    tb::MonoTime last_flush_ = tb::MonoClock::now();
};

/// Multiple sinks
template<class Self, class ValueL,  template<class...> class SinkM, typename...O>
class BasicClickHouseService: public io::BasicMultiService<Self, ClickHouseConn,  mp::mp_transform<SinkM, ValueL>, O...>
, public BasicSignalSlot<Self>
{
    FT_SELF(Self);
    using Base = io::BasicMultiService<Self, ClickHouseConn, mp::mp_transform<SinkM, ValueL>, O...>;
    using SignalSlot = BasicSignalSlot<Self>;
  public:
    using Base::Base;
    using Base::url;
    using Base::parent;
    using Base::open, Base::close;
    using Base::for_each_service;

    template<typename...ArgsT>
    BasicClickHouseService(core::InstrumentsCache* ins=nullptr, ArgsT...args)
    : instruments_(ins)
    , Base(std::forward<ArgsT>(args)...) {

    }
    template<class ServiceT>
    void on_new_service(ServiceT& svc) {
        svc.instruments(instruments_);
    }

    void instruments(core::InstrumentsCache* val) { instruments_ = val; }
    core::InstrumentsCache* instruments() { return instruments_; }

    template<class ValueT, typename...ArgsT>
    auto* service(const typename SinkM<ValueT>::Endpoint& endpoint, ArgsT...args) {
        return Base::template service<SinkM<ValueT>, ArgsT...>(endpoint, std::forward<ArgsT>(args)...);
    }


    core::Stream& slot(core::StreamTopic topic) { 
      switch(topic) {
        case core::StreamTopic::BestPrice:
          return *service<core::Tick>(topic);
        default: return SignalSlot::slot(topic);
      }
    }

    void on_parameters_updated(const core::Parameters& params) {
        url(params.value<std::string_view>("url"));
        Base::on_parameters_updated(params);    // will set url
        populate(params);
    }

    void populate(const core::Parameters& params) {
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
        ClickHouseConn::open();
    }

    void do_close() {
        Base::do_close();
        ClickHouseConn::close();
    }
protected:
    core::InstrumentsCache* instruments_{};
};

template<class ValueT, class FieldsT=JoinFields<BasicFields<core::Field>>, typename...O>
class ClickHouseSink : public BasicClickHouseSink<ClickHouseSink<ValueT, FieldsT, O...>, ValueT, FieldsT, O...>
{
    using Base = BasicClickHouseSink<ClickHouseSink<ValueT, FieldsT, O...>, ValueT, FieldsT, O...>;
public:
    using typename Base::Type;
    using Base::Base;
    using Base::fields, Base::instruments;

}; 

template<class ValueL, template<class...> class SinkM=ClickHouseSink>
class ClickHouseService: public BasicClickHouseService<ClickHouseService<ValueL, SinkM>,  ValueL, SinkM>
{
    using Base = BasicClickHouseService<ClickHouseService<ValueL, SinkM>, ValueL, SinkM>;
public:
    using Base::Base;
};

};