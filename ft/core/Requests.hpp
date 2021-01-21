#pragma once

#include "ft/capi/ft-types.h"
#include "ft/core/Identifiable.hpp"
#include "ft/core/Stream.hpp"
#include "toolbox/net/Endpoint.hpp"
#include "toolbox/sys/Time.hpp"
#include "ft/sbe/SbeTypes.hpp"
#include "ft/core/Subscription.hpp"
#include "toolbox/util/Slot.hpp"

namespace ft { inline namespace core {

#pragma pack(push, 1)

enum class Request : ft_event_t {
    Subscribe       = FT_REQ_SUBSCRIBE,
    Unsubscribe     = FT_REQ_UNSUBSCRIBE,
    UnsubscribeAll  = FT_REQ_UNSUBSCRIBE_ALL
};

inline std::ostream& operator<<(std::ostream& os, Request req) {
    switch(req) {
        case Request::Subscribe: return os << "Subscribe";
        case Request::Unsubscribe: return os << "Unsubscribe";
        case Request::UnsubscribeAll: return os <<"UnsubscribeAll";
        default: return os << tb::unbox(req);
    }
}

enum class Response : ft_event_t {
    Subscribe       = FT_RES_SUBSCRIBE,
    Unsubscribe     = FT_RES_UNSUBSCRIBE,
    UnsubscribeAll  = FT_RES_UNSUBSCRIBE_ALL
};

template<typename ImplT>
class BasicHeader {
public:
    BasicHeader(ImplT& impl) : impl_(impl){}
    std::size_t length() { return impl_.ft_len; };
    void length(std::size_t len) { impl_.ft_len = len; }
    ft_event_t event() const { return impl_.ft_type.ft_event; } 
    ft_topic_t topic() const { return impl_.ft_type.ft_topic; }
    ft_seq_t sequence() const { return impl_.ft_seq; }
    void sequence(ft_seq_t seq) { impl_.ft_seq = seq; }
    tb::WallTime recv_time() const { return tb::WallTime(tb::Nanos(impl_.ft_recv_time)); }
    tb::WallTime send_time() const { return tb::WallTime(tb::Nanos(impl_.ft_send_time)); }
private:
    ImplT& impl_;
};

using Header = BasicHeader<ft_hdr_t>;

using RequestId = Identifier;

template<class BaseT>
class BasicRequestResponse : public BaseT {
  using Base = BaseT;
public:
    using Base::Base;
    using RequestId = decltype(Base::ft_request_id);

    void reset() {
        std::memset(this, 0, sizeof(*this));
    }

    core::Header header() const { return core::Header{Base::ft_hdr}; }
    
    std::size_t bytesize() const { return Base::ft_hdr.ft_len; };
    void bytesize(std::size_t len) { Base::ft_hdr.ft_len = len; }
        
    ft_topic_t topic() const { return Base::ft_hdr.ft_type.ft_topic; }
    void topic(ft_topic_t val) { Base::ft_hdr.ft_type.ft_topic = val; }
    
    ft_seq_t sequence() const { return Base::ft_hdr.ft_seq; }
    void sequence(ft_seq_t seq) { Base::ft_hdr.ft_seq = seq; }

    RequestId request_id() const { return Base::ft_request_id; }
    void request_id(const RequestId val) { Base::ft_request_id = val; }

    core::InstrumentId instrument_id() const { return Base::ft_instrument_id; }
    void instrument_id(core::InstrumentId val) { Base::ft_instrument_id = val; }
    
    tb::WallTime recv_time() const { return tb::WallTime(tb::Nanos(Base::ft_hdr.ft_recv_time)); }
    tb::WallTime send_time() const { return tb::WallTime(tb::Nanos(Base::ft_hdr.ft_send_time)); }
};

template<class ResponseT>
using ResponseSlot = tb::Slot<const ResponseT&, std::error_code>;


template<class BaseT>
class BasicRequest : public BasicRequestResponse<BaseT> {
    using Base = BasicRequestResponse<BaseT>;
public:
    using Base::Base;
    void reset() {
        std::memset(this, 0, sizeof(*this));
    }
    core::Request request() const { return static_cast<core::Request>(Base::ft_hdr.ft_type.ft_event); } 
    void request(core::Request val) { Base::ft_hdr.ft_type.ft_event = tb::unbox(val); }
    
    friend std::ostream& operator<<(std::ostream& os, const BasicRequest<BaseT>& self) {
        return os<<"event:'"<<self.request()<<"',topic:"<<(int)self.topic()<<",req:"<<self.request_id()<<",iid:"<<self.instrument_id();
    }
};

enum class Status : ft_status_t {
    Empty = FT_STATUS_UNKNOWN,
    Pending = FT_STATUS_PENDING,
    Ok = FT_STATUS_OK,
    Failed = FT_STATUS_FAILED
};

template<class BaseT>
class BasicResponse : public BasicRequestResponse<BaseT> {
    using Base = BasicRequestResponse<BaseT>;
    using This = BasicResponse<BaseT>;
public:
    using String = sbe::BasicVarString<ft_slen_t>;
    using typename Base::RequestId;
    //using SlotMap = tb::SignalMap<const This&, std::error_code>;
    using Slot = tb::Slot<const This&, std::error_code>;

    using Base::Base;
    
    void reset() {
        std::memset(this, 0, sizeof(*this));
    }
    core::Status status() const { return core::Status(Base::ft_status);}
    void status(core::Status val) const { return Base::ft_status = tb::unbox(val);}
    core::Response response() const { return static_cast<core::Response>(Base::ft_hdr.ft_type.ft_event); } 
    void response(core::Response val) { Base::ft_hdr.ft_type.ft_event = tb::unbox(val); }
    String& message() { return *reinterpret_cast<String*>(&Base::ft_message_len); } 
};

using StatusResponse = BasicResponse<ft_response_t>;


#define rel_offsetof(t, d, s) offsetof(t,d)-offsetof(t,s)

constexpr static std::size_t MaxSymbolSize = 64;


template<std::size_t SizeI>
class BasicSubscriptionRequest : public core::BasicRequest<ft_subscribe_t> {
    using Base = core::BasicRequest<ft_subscribe_t>;
public:
    using Response = core::StatusResponse;

    using Symbol = sbe::BasicVarString<ft_slen_t, rel_offsetof(ft_subscribe_t,ft_symbol,ft_symbol_len)>;
    const Symbol& symbol_data() const  {
        return *reinterpret_cast<const Symbol*>(&ft_symbol_len);
    }
    Symbol& symbol_data()  {
        return *reinterpret_cast<Symbol*>(&ft_symbol_len);
    }
    std::string_view symbol() const {
        return symbol_data().str();
    }
    void symbol(std::string_view val) {
        symbol_data() = val;
    }
    friend std::ostream& operator<<(std::ostream& os, const BasicSubscriptionRequest<SizeI>& self) {
        return os << static_cast<const Base&>(self)<<",sym:'"<<self.symbol()<<"'";
    }
private:
    char data_[SizeI];
};

using SubscriptionRequest = BasicSubscriptionRequest<MaxSymbolSize>;
using SubscriptionSignal = tb::Signal<PeerId, const core::SubscriptionRequest&>;

#pragma pack(pop)
}} // ft::core