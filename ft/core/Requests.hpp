#pragma once

#include "ft/capi/ft-types.h"
#include "ft/core/Identifiable.hpp"
#include "ft/core/Stream.hpp"
#include "toolbox/sys/Time.hpp"
#include "ft/sbe/SbeTypes.hpp"
#include "ft/core/MdSub.hpp"

namespace ft { inline namespace core {

enum class RequestType:uint8_t {
    Subscribe       = FT_SUBSCRIBE,
    Unsubscribe     = FT_UNSUBSCRIBE,
    UnsubscribeAll  = FT_UNSUBSCRIBE_ALL
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

template<class BaseT>
class BasicRequest : public BaseT {
  using Base = BaseT;
public:
    core::Header header() { return core::Header{Base::ft_hdr}; }
    
    std::size_t length() { return Base::ft_hdr.ft_len; };
    void length(std::size_t len) { Base::ft_hdr.ft_len = len; }
    
    core::RequestType request_type() const { return static_cast<core::RequestType>(Base::ft_hdr.ft_type.ft_event); } 
    void request_type(core::RequestType val) { Base::ft_hdr.ft_type.ft_event = tb::unbox(val); }
    
    ft_topic_t topic() const { return Base::ft_hdr.ft_type.ft_topic; }
    void topic(ft_topic_t val) { Base::ft_hdr.ft_type.ft_topic = val; }
    
    ft_seq_t sequence() const { return Base::ft_hdr.ft_seq; }
    void sequence(ft_seq_t seq) { Base::ft_hdr.ft_seq = seq; }
    
    tb::WallTime recv_time() const { return tb::WallTime(tb::Nanos(Base::ft_hdr.ft_recv_time)); }
    tb::WallTime send_time() const { return tb::WallTime(tb::Nanos(Base::ft_hdr.ft_send_time)); }
};

#define rel_offsetof(t, d, s) offsetof(t,d)-offsetof(t,s)

constexpr static std::size_t MaxSymbolSize = 64;

template<std::size_t SizeI>
class BasicSubscriptionRequest : public core::BasicRequest<ft_subscribe_t> {
    using Base = core::BasicRequest<ft_subscribe_t>;
public:
    using SymbolString = sbe::BasicVarString<ft_slen_t, rel_offsetof(ft_subscribe_t,ft_symbol,ft_symbol_len)>;
    SymbolString symbol() {
        return SymbolString {};
    }
    core::InstrumentId instrument_id() const {
        return ft_instrument_id;
    }

private:
    char data_[SizeI];
};


using SubscriptionRequest = BasicSubscriptionRequest<MaxSymbolSize>;
using SubscriptionSignal = tb::Signal<const core::SubscriptionRequest&>;

}} // ft::core