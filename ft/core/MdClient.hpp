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
    virtual void async_request(const core::SubscriptionRequest& req, core::SubscriptionResponse::Slot slot, tb::SizeSlot done) = 0;
};



struct MdClientTraits {
    template<typename T>
    using async_subscribe_t = decltype(std::declval<T&>().async_request(
        std::declval<const core::SubscriptionRequest&>(), 
        std::declval<tb::Slot<const core::SubscriptionResponse&, std::error_code>>(), 
        std::declval<tb::SizeSlot>()));
    template<typename T>
    constexpr static  bool has_async_subscribe = boost::is_detected_v<async_subscribe_t, T>;

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
        if constexpr (MdClientTraits::has_async_connect<Impl>) {
            impl()->async_connect(done);
        } else {
            done({});
        }
    }

    void async_request(const core::SubscriptionRequest& req
    , tb::Slot<const SubscriptionResponse&, std::error_code> slot
    , tb::SizeSlot done) override
    {
        if constexpr(MdClientTraits::has_async_subscribe<Impl>) {
            impl()->async_request(req, slot, done);
        } else {
            done(-1, make_error_code(std::errc::not_supported));
        }
    }
protected:
    Impl impl_;
};



}} // ft::core