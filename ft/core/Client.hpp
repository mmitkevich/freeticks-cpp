#pragma once

#include "ft/core/Instrument.hpp"
#include "ft/core/Requests.hpp"
#include "ft/core/Stream.hpp"
#include "ft/core/StreamStats.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/Tick.hpp"
#include "ft/utils/Common.hpp"
#include "ft/core/Component.hpp"
#include "toolbox/util/Slot.hpp"
#include "ft/core/Service.hpp"
#include <system_error>


namespace ft { inline namespace core {

class IClient : public core::IService {
public:    
    FT_IFACE(IClient);
    /// asynchronous connect
    virtual void async_connect(tb::DoneSlot done) = 0;
    virtual void async_write(const core::SubscriptionRequest& req, tb::SizeSlot done) = 0;
    template<typename...ArgsT>
    Stream::Signal<ArgsT...>& signal_of(StreamTopic topic) {
        return Stream::Signal<ArgsT...>::from(this->signal(topic));
    }
};



template<class RequestT>
struct RequestTraits {
    template<typename T>
    using async_write_t = decltype(std::declval<T&>().async_write(
        std::declval<const core::SubscriptionRequest&>(), 
        std::declval<tb::SizeSlot>()));
    template<typename T>
    constexpr static  bool has_async_write = boost::is_detected_v<async_write_t, T>;
};

struct ClientTraits {
    template<typename T>
    using async_connect_t = decltype(std::declval<T&>().async_connect(std::declval<tb::DoneSlot>()));
    template<typename T>
    constexpr static  bool has_async_connect = boost::is_detected_v<async_connect_t, T>;
};

// wrap MD client into dynamic interface
template<class SelfT, class Base>
class IClient::Impl: public core::IService::Impl<IClient::Impl<SelfT,Base>, Base>
{
public:
    FT_IMPL(SelfT);
    //core::StreamStats& stats() override { return impl()->stats(); }
    void async_connect(tb::DoneSlot done) override {
        if constexpr (ClientTraits::has_async_connect<decltype(*impl())>) {
            impl()->async_connect(done);
        } else {
            done({});
        }
    }
    void async_write(const core::SubscriptionRequest& req, tb::SizeSlot done) override
    {
        if constexpr(RequestTraits<core::SubscriptionRequest>::has_async_write<decltype(*impl())>) {
            impl()->async_write(req, done);
        } else {
            done(-1, make_error_code(std::errc::not_supported));
        }
    }
};



}} // ft::core