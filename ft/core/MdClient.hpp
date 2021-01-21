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


namespace streams {
    constexpr static const char* BestPrice = "BestPrice";
    constexpr static const char* Instrument = "Instrument";
};


class IMdClient : public core::IService {
public:    
    virtual TickStream& ticks(StreamTopic topic) = 0;

    virtual InstrumentStream& instruments(StreamTopic topic) = 0;

    // very basic stats
    virtual core::StreamStats& stats() = 0;

    virtual void async_connect(tb::DoneSlot done) = 0;
    virtual void async_write(const core::SubscriptionRequest& req, tb::SizeSlot done) = 0;
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
template<class Impl>
class MdClientImpl: public core::ServiceImpl<MdClientImpl<Impl>, IMdClient>
{
    using Base = core::ServiceImpl<MdClientImpl<Impl>, IMdClient>;
public:
    template<typename...ArgsT>
    MdClientImpl(ArgsT...args)
    : impl_(std::forward<ArgsT>(args)...)
    {}

    auto* impl() { return &impl_; }
    const auto* impl() const { return &impl_; }

    core::StreamStats& stats() override { return impl()->stats(); }

    core::TickStream& ticks(StreamTopic topic) override { return impl()->ticks(topic); }
    core::InstrumentStream& instruments(StreamTopic topic) override { return impl()->instruments(topic); }

    void async_connect(tb::DoneSlot done) override {
        if constexpr (ClientTraits::has_async_connect<Impl>) {
            impl()->async_connect(done);
        } else {
            done({});
        }
    }

    void async_write(const core::SubscriptionRequest& req, tb::SizeSlot done) override
    {
        if constexpr(RequestTraits<core::SubscriptionRequest>::has_async_write<Impl>) {
            impl()->async_write(req, done);
        } else {
            done(-1, make_error_code(std::errc::not_supported));
        }
    }
protected:
    Impl impl_;
};



}} // ft::core