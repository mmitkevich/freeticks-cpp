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
#include <system_error>


namespace ft { inline namespace core {


namespace streams {
    constexpr static const char* BestPrice = "BestPrice";
    constexpr static const char* Instrument = "Instrument";
};


class IMdClient : public IComponent {
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
    using async_request_t = decltype(std::declval<T&>().async_request(
        std::declval<const core::SubscriptionRequest&>(), 
        std::declval<tb::Slot<const core::SubscriptionResponse&, std::error_code>>(), 
        std::declval<tb::SizeSlot>()));
    template<typename T>
    constexpr static  bool has_async_request = boost::is_detected_v<async_request_t, T>;
};

// wrap MD client into dynamic interface
template<typename ImplT>
class MdClientImpl : public IMdClient {
public:
    template<typename...ArgsT>
    MdClientImpl(ArgsT...args)
    : impl_(std::forward<ArgsT>(args)...) {}

    void start() override { impl_.start(); }
    void stop() override { impl_.stop(); }
    
    void url(std::string_view url) { impl_.url(url);}
    std::string_view url() const { return impl_.url(); }

    State state() const override { return impl_.state(); }
    StateSignal& state_changed() override { return impl_.state_changed(); };

    core::StreamStats& stats() override { return impl_.stats(); }

    void parameters(const Parameters& parameters, bool replace=false) override { impl_.parameters(parameters, replace); }
    const Parameters& parameters() const override { return impl_.parameters(); }
    
    core::TickStream& ticks(StreamTopic topic) override { return impl_.ticks(topic); }
    core::InstrumentStream& instruments(StreamTopic topic) override { return impl_.instruments(topic); }

    template<typename SockT>
    using async_connect_t = decltype(std::declval<SockT&>().async_connect(std::declval<tb::DoneSlot>()));
    template<typename SockT>
    constexpr static  bool has_async_connect = boost::is_detected_v<async_connect_t, SockT>;

    void async_connect(tb::DoneSlot done) override {
        if constexpr (has_async_connect<ImplT>) {
            impl_.async_connect(done);
        } else {
            done({});
        }
    }

    void async_request(const core::SubscriptionRequest& req, tb::Slot<const SubscriptionResponse&, std::error_code> slot, tb::SizeSlot done) override {
        if constexpr(MdClientTraits::has_async_request<ImplT>) {
            impl_.async_request(req, slot, done);
        } else {
            done(-1, make_error_code(std::errc::not_supported));
        }
    }
private:
    ImplT impl_;
};



}} // ft::core