#pragma once

#include "ft/capi/ft-types.h"
#include "ft/core/Stream.hpp"
#include "toolbox/sys/Time.hpp"

namespace ft::core {

enum class RequestType {
    Subscribe       = FT_SUBSCRIBE,
    Unsubscribe     = FT_UNSUBSCRIBE,
};


template<typename ImplT>
class BasicHeader {
public:
    BasicHeader(ImplT& impl) : impl_(impl){}
    std::size_t length() { return impl_.ft_len; };
    void length(std::size_t len) { impl_.t_len = len; }
    ft_event_t type() const { return impl_.ft_type.ft_event; } 
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
};

class SubscriptionRequest : public core::BasicRequest<ft_subscribe_t> {
    using Base = core::BasicRequest<ft_subscribe_t>;
public:
};

}